package main

import (
	"fmt"
	"image"
	"image/draw"
	"image/jpeg"
	"io/fs"
	"os"
	"os/exec"
	"strconv"
	"strings"
)

// 1. create cli <resolution> <fps> <directory>
//
// 2. scale all the images in the directory
//
// 3. translate single image across the viewport
//
// 4. translate two images to seamlessly animate across the viewport
// 41. I just made a really large image and traslated that
//
// 5. animate all images across the viewport
//
// 6. export the image sequence as an mp4
//
// 7. Add ability to select between png and jpeg
//
// 8. Add ability to use the poppler to convert a pdf all the way to a mp4

func main() {
	args := os.Args

	var resolution []int = []int{1920, 1080}
	var fps int = 30
	var secondsPerPage float64 = 6
	var videoFormat string = ""
	var pdfPath string = ""
	var keepContents bool = false
	var animationStyle string = ""

	helpStr := "Images to Video\n" +
		"Dependencies: imageMagick, poppler, ffmpeg\n" +
		"Usage: ptv [flags] [path_to_pdf]\n" +
		"    -h                        : help\n" +
		"    -r <int> <int>            : set output resolution, default: 1920 1080\n" +
		"    -s <float>                : duration for each page to be displayed in seconds, default: 6.0\n" +
		"    -fps <int>                : sets fps, default: 30\n" +
		"    -style <string>           : animates the frames in image 'sequence' or 'scroll' style\n" +
		"    -keep                     : don't delete pdf contents directory after encoding video\n" +
		"    -avi                      : exports to .avi\n" +
		"    -mov                      : exports to .mov\n" +
		"    -mp4                      : exports to .mp4"

	// CLI
	if len(args) == 1 {
		fmt.Println(helpStr)
		return
	}

	for i := 1; i < len(args); i++ {
		if args[i] == "-h" {
			fmt.Println(helpStr)
			return
		} else if args[i] == "-r" {
			i++
			resolutionX, err := strconv.Atoi(args[i])
			if err != nil {
				fmt.Println("Error parsing resolution width value:", err)
				return
			}
			resolution[0] = resolutionX
			i++
			resolutionY, err := strconv.Atoi(args[i])
			if err != nil {
				fmt.Println("Error parsing resolution height value:", err)
				return
			}
			resolution[1] = resolutionY
		} else if args[i] == "-fps" {
			i++
			fps_, err := strconv.Atoi(args[i])
			if err != nil {
				fmt.Println("Error parsing fps value: ", err)
				return
			}
			fps = fps_
		} else if args[i] == "-s" {
			i++
			secondsPerPage_, err := strconv.ParseFloat(args[i], 64)
			if err != nil {
				fmt.Println("Error parsing seconds:", err)
				return
			}
			secondsPerPage = secondsPerPage_
		} else if args[i] == "-style" {
			i++
			if args[i] != "scroll" && args[i] != "sequence" {
				fmt.Println("Error: animation style not valid. Use either 'scroll' or 'sequence'")
				return
			}
			animationStyle = args[i]
		} else if args[i] == "-keep" {
			keepContents = true
		} else if args[i] == "-avi" || args[i] == "-mov" || args[i] == "-mp4" {
			videoFormat = strings.TrimPrefix(args[i], "-")
		} else if strings.Contains(args[i], "/") {
			pdfPath_ := args[i]
			if strings.LastIndex(pdfPath_, ".pdf") != len(pdfPath_)-4 {
				fmt.Println("Error: not a PDF file.", pdfPath_)
				return
			}
			_, err := os.Stat(pdfPath_)
			if err != nil {
				if os.IsNotExist(err) {
					fmt.Println("PDF path dosen't exist:", pdfPath_)
					return
				} else {
					fmt.Println("Error checking PDF path:", err)
					return
				}
			} else {
				fmt.Println("PDF path exists:", pdfPath_)
			}
			pdfPath = pdfPath_
		} else {
			fmt.Println("Invalid argument:", args[i])
			return
		}
	}

	// argument checks
	if pdfPath == "" {
		fmt.Println("Error: PDF path not stated. Use -h for help")
		return
	}
	if videoFormat == "" {
		fmt.Println("Error: No video format to export to. Use -h for help")
		return
	}
	if animationStyle != "scroll" && animationStyle != "sequence" {
		fmt.Println("Error: animation style not defined. -h for help")
		return
	}

	// convert the pdf pages to images
	imagesDir := pdfToImages(pdfPath)

	// convert pdf to a video
	if animationStyle == "scroll" {
		imagesToVideoScroll(imagesDir, fps, secondsPerPage, resolution, videoFormat)
	} else if animationStyle == "sequence" {
		imagesToVideoSequence(imagesDir, fps, secondsPerPage, resolution, videoFormat)
	}

	// keep or remove the content generated from the pdf
	if !keepContents {
		err := os.RemoveAll(imagesDir)
		if err != nil {
			fmt.Println("Error deleting PDF directory:", err)
		} else {
			fmt.Println("Directory containing PDF contents has been deleted.", imagesDir)
		}
	} else {
		fmt.Println("Keeped directory containing PDF contents.", imagesDir)
	}

	fmt.Println("This is PDF to Video")
}

func pdfToImages(pdfPath string) string {

	pdfDir := makePDFDir(pdfPath)

	var args []string = []string{
		"-progress",
		"-aa",
		"yes",
		"-aaVector",
		"yes",
		"-jpeg",
		"-r",
		"150",
		"-sep",
		"0",
		pdfPath,
		pdfDir,
	}

	result := exec.Command("pdftoppm", args...)
	output, err := result.Output()
	if err != nil {
		fmt.Println("Output:", output)
		fmt.Println("Error converting pdf to images:", err)
	}

	// scale factor so image fits inside viewport
	return pdfDir
}

func imagesToVideoSequence(dir string, fps int, secondsPerPage float64, res []int, vf string) {
	var framesDir string = makeFramesDir(dir)

	files := getImageDirEntrys(dir)
	img := openImage(dir + files[0].Name())

	// scale images
	scaleRatio := float64(res[1]) / float64(img.Bounds().Dx())
	scaleImages(files, dir, scaleRatio)

	// duplicated frames for the given fps + secondsPerPage
	count := 0
	for _, file := range files {
		imgName := file.Name()
		for i := 0; i < int(float64(fps)*secondsPerPage); i++ {

			var arguments []string = []string{
				dir + imgName,
				framesDir + getExportName(count, ".jpg"),
			}

			result := exec.Command("cp", arguments...)
			_, err := result.Output()
			if err != nil {
				fmt.Println("Error creating frame in sequence.", err)
			}

			count++
		}
	}

	// encode to video
	videoPath := strings.TrimSuffix(dir, "_pdf/")
	fmt.Println("Video Path:", videoPath)
	fmt.Println("Encoding frames into ." + vf + " file.")
	// convert image sequence to a video
	// ffmpeg -f image2 -framerate 12 -i ./%04d.jpg -s 1920x1080 e.mp4
	// The syntax foo-%03d.jpeg specifies to use a decimal number composed of three digits padded with zeroes to express the sequence number.
	var arguments []string = []string{
		"-f",
		"image2",
		"-framerate",
		strconv.Itoa(fps),
		"-i",
		framesDir + "%05d.jpg",
		"-s",
		strconv.Itoa(res[0]) + "x" + strconv.Itoa(res[1]),
		videoPath + "." + vf,
	}
	cmdResult := exec.Command("ffmpeg", arguments...)
	_, err := cmdResult.Output()
	if err != nil {
		fmt.Println("Error converting images to video:", err)
		return
	}
}

func imagesToVideoScroll(dir string, fps int, secondsPerPage float64, res []int, vf string) {
	var pixelsTranslated int
	var framesDir string = makeFramesDir(dir)
	var frameCount int
	// create viewport
	var viewportWidth int = res[0]
	var viewportHeight int = res[1]
	viewport := image.NewRGBA(image.Rect(0, 0, viewportWidth, viewportHeight))

	files := getImageDirEntrys(dir)
	img := openImage(dir + files[0].Name())

	// scale images based on animation style
	scaleRatio := float64(res[0]) / float64(img.Bounds().Dx())
	scaleImages(files, dir, scaleRatio)

	// get files based on them being scaled
	sFiles := getImageDirEntrys(dir)
	sImg := openImage(dir + sFiles[0].Name())

	// creates longImg
	longImg := image.NewRGBA(image.Rect(0, 0, viewportWidth, sImg.Bounds().Max.Y*len(files)))

	// adds image data to longImg
	nextPoint := image.Pt(0, 0)
	for i := 0; i < len(files); i++ {
		nextImg := openImage(dir + files[i].Name())

		draw.Draw(longImg, longImg.Rect, nextImg, nextPoint, draw.Src)

		nextPoint.Y -= nextImg.Bounds().Max.Y
	}

	// frames of the video
	frameCount = int(float64(fps) * secondsPerPage * float64(len(files)))
	fmt.Println("Total frames (approximation):", frameCount)

	// calculate pixelsTranslated per draw
	pixelsTranslated = (longImg.Bounds().Dy() + (2 * viewportHeight)) / (frameCount)
	fmt.Println("Pixels translated per frame:", pixelsTranslated)

	fmt.Println("Creating video frames...")
	// animates longImg on viewport
	for posY, count := -viewportHeight, 0; posY <= longImg.Bounds().Dy()+pixelsTranslated; posY += pixelsTranslated {
		// scaled image draw location in viewport
		point := image.Pt(0, posY)

		// draws and saves frame
		draw.Draw(viewport, viewport.Bounds(), longImg, point, draw.Src)
		saveJPEG(viewport, framesDir+getExportName(count, ".jpg"))

		// reset viewport
		draw.Draw(viewport, viewport.Bounds(), image.Black, image.Pt(0, 0), draw.Src)

		count++
	}

	videoPath := strings.TrimSuffix(dir, "_pdf/")
	fmt.Println("Encoding frames into ." + vf + " file.")
	// convert image sequence to a video
	// ffmpeg -f image2 -framerate 12 -i ./%04d.jpg -s 1920x1080 e.mp4
	// The syntax foo-%03d.jpeg specifies to use a decimal number composed of three digits padded with zeroes to express the sequence number.
	var arguments []string = []string{
		"-f",
		"image2",
		"-framerate",
		strconv.Itoa(fps),
		"-i",
		framesDir + "%05d.jpg",
		"-s",
		strconv.Itoa(res[0]) + "x" + strconv.Itoa(res[1]),
		videoPath + "." + vf,
	}
	cmdResult := exec.Command("ffmpeg", arguments...)
	_, err := cmdResult.Output()
	if err != nil {
		fmt.Println("Error converting sequence to video:", err)
		return
	}

}

func saveJPEG(img image.Image, fileName string) error {
	f, err := os.Create(fileName)
	if err != nil {
		return err
	}

	defer f.Close()

	return jpeg.Encode(f, img, &jpeg.Options{Quality: 100})
}

func getImageDirEntrys(dir string) []fs.DirEntry {
	content, err := os.ReadDir(dir)
	var files []fs.DirEntry
	if err != nil {
		fmt.Println("Error reading directory:", err)
	}
	for i := 0; i < len(content); i++ {
		if strings.Index(content[i].Name(), ".") != -1 {
			files = append(files, content[i])
		}
	}
	return files
}

func openImage(path string) image.Image {
	file, err := os.Open(path)
	if err != nil {
		fmt.Println("Error opening file:", err)
	}
	img, err := jpeg.Decode(file)
	if err != nil {
		fmt.Println("Error decoding image:", err)
	}
	return img
}

func getExportName(count int, fileExtension string) string {
	var result string

	if count < 10 {
		result = "0000" + strconv.Itoa(count)
	} else if count < 100 {
		result = "000" + strconv.Itoa(count)
	} else if count < 1000 {
		result = "00" + strconv.Itoa(count)
	} else if count < 10000 {
		result = "0" + strconv.Itoa(count)
	} else {
		result = strconv.Itoa(count)
	}
	result += fileExtension

	return result
}

func scaleImages(files []fs.DirEntry, dir string, scaleRatio float64) {
	scalePercentage := strconv.FormatFloat(scaleRatio*100.0, 'f', 4, 64) + "%"

	fmt.Println("Scalling images...")
	// scalles all images in the directory
	for i := 0; i < len(files); i++ {
		filePath := dir + files[i].Name()

		var arguments []string = []string{
			filePath,
			"-resize",
			scalePercentage,
			filePath,
		}
		// cmd to scale image
		cmdResult := exec.Command("magick", arguments...)
		_, err := cmdResult.Output()
		if err != nil {
			fmt.Println("Error resizing image:", err)
		}
	}

	fmt.Println("Scaled images.")
}

func makePDFDir(pdfPath string) string {
	pdfDir := strings.TrimSuffix(pdfPath, ".pdf")
	pdfDir += "_pdf/"

	err := os.Mkdir(pdfDir, 0755)
	if err != nil {
		if os.IsExist(err) {
			fmt.Println("PDF directory already exists:", pdfDir)
		} else {
			fmt.Println("Error making PDF directory:", err)
		}
	} else {
		fmt.Println("Created directory for PDF contents:", pdfDir)
	}

	return pdfDir
}

func makeFramesDir(dir string) string {
	framesDir := dir + "frames/"

	err := os.Mkdir(framesDir, 0755)
	if err != nil {
		if os.IsExist(err) {
			fmt.Println("Frames directory already exists:", framesDir)
		} else {
			fmt.Println("Error making frames directory:", err)
		}
	} else {
		fmt.Println("Created directory for video frames:", framesDir)
	}

	return framesDir
}
