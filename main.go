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
// USE GO RUTINES !!!
//
// 5. animate all images across the viewport
//
// 6. export the image sequence as an mp4

func main() {

	args := os.Args

	var directory string
	var resolution []int = []int{1920, 1080}
	var fps int = 30
	var secondsPerPage int = 6

	helpStr := "Images to Video\n" +
		"itv [options] [path_to_dir]\n" +
		"    -r <int> <int>            : defins output resolution -> [width] [height]\n" +
		"    -fps <int>                : sets fps\n" +
		"    -s <int>                  : duration for each page to be displayed for in seconds\n" +
		"    -h                        : help"

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
			fps = fps_
			if err != nil {
				fmt.Println("Error parsing fps value: ", err)
				return
			}
		} else if strings.Contains(args[i], "/") {
			directory_ := args[i]
			directory = directory_
			if directory[len(directory)-1] != '/' {
				fmt.Println("Error: not a directory.")
				return
			}
			_, err := os.Stat(directory)
			if err != nil {
				if os.IsNotExist(err) {
					fmt.Println("Directory does not exist.")
					return
				} else {
					fmt.Println("Error checking directory:", err)
					return
				}
			} else {
				fmt.Println("Source directory exists:", directory)
			}
		} else {
			fmt.Println("Invalid argument:", args[i])
			return
		}
	}

	imagesToVideo(directory, fps, secondsPerPage, resolution)

	fmt.Println("This is Image to Video")
}

func imagesToVideo(dir string, fps int, secondsPerPage int, res []int) {

	var pixelsTranslated int
	var scaledDir string = dir + "scaled/"
	var exportDir string = dir + "export/"
	// create viewport
	var viewportWidth int = res[0]
	var viewportHeight int = res[1]
	viewport := image.NewRGBA(image.Rect(0, 0, viewportWidth, viewportHeight))

	// image entrys
	files := getImageDirEntrys(dir)
	img := openImage(dir + files[0].Name())

	// scale factor so image fits inside viewport
	scalePercentage := float64(viewportWidth) / float64(img.Bounds().Dx())

	// Open scaled images
	scaledFiles := scaleImages(files, dir, scaledDir, scalePercentage)
	scaledImg := openImage(scaledDir + scaledFiles[0].Name())

	// checks if image scalled correctly
	if scaledImg.Bounds().Dx() == viewportWidth {
		fmt.Println("Images scaled correctly")
		// fmt.Println(scaledImg.Bounds().Dx())
	} else {
		fmt.Println("Images didn't scale correctly")
	}

	// creates longImg
	sImgHeight := scaledImg.Bounds().Dy()
	longImg := image.NewRGBA(image.Rect(0, 0, viewportWidth, sImgHeight*len(scaledFiles)))

	// adds image data to longImg
	sPoint := image.Pt(0, 0)
	for i := 0; i < len(scaledFiles); i++ {
		sImg := openImage(scaledDir + scaledFiles[i].Name())

		draw.Draw(longImg, longImg.Rect, sImg, sPoint, draw.Src)

		sPoint.Y -= sImgHeight
	}
	// saveJPEG(longImg, "long.jpg")

	// calculate pixelsTranslated per draw
	pixelsTranslated = (sImgHeight + viewportHeight) / (fps * secondsPerPage)

	err := os.Mkdir(exportDir, 0755)
	if err != nil {
		if os.IsExist(err) {
			fmt.Println("Export directory exists:", exportDir)
		} else {
			fmt.Println("Export directory dose not exist:", err)
		}
	} else {
		fmt.Println("Created export directory:", exportDir)
	}

	// animates longImg on viewport
	for posY, count := -viewportHeight, 0; posY <= longImg.Bounds().Dy()+pixelsTranslated; posY += pixelsTranslated {
		// scaled image draw location in viewport
		point := image.Pt(0, posY)

		// draws and saves frame
		draw.Draw(viewport, viewport.Bounds(), longImg, point, draw.Src)
		saveJPEG(viewport, exportDir+getExportName(count, ".jpg"))

		// reset viewport
		draw.Draw(viewport, viewport.Bounds(), image.Black, image.Pt(0, 0), draw.Src)

		count++
	}
}

// FUNCTIONS

func saveJPEG(img image.Image, fileName string) error {
	f, err := os.Create(fileName)
	if err != nil {
		return err
	}

	defer f.Close()

	return jpeg.Encode(f, img, &jpeg.Options{Quality: 100})
}

func getImageDirEntrys(dir string) []fs.DirEntry {
	files, err := os.ReadDir(dir)
	if err != nil {
		fmt.Println("Error reading directory:", err)
	}
	// for _, file := range files {
	// 	fmt.Println(file.Name())
	// }
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
		result = "000" + strconv.Itoa(count)
	} else if count < 100 {
		result = "00" + strconv.Itoa(count)
	} else if count < 1000 {
		result = "0" + strconv.Itoa(count)
	} else {
		result = strconv.Itoa(count)
	}
	result += fileExtension

	return result
}

func scaleImages(files []fs.DirEntry, dir string, scaledDir string, scalePct float64) []fs.DirEntry {
	err := os.Mkdir(scaledDir, 0775)
	if err != nil {
		if os.IsExist(err) {
			fmt.Println("Scaled directory exists:", scaledDir)
			return getImageDirEntrys(scaledDir)
		} else {
			fmt.Println("Error making scalded directory:", err)
		}
	} else {
		fmt.Println("Created scaled directory:", scaledDir)
	}

	scalePctFormated := strconv.FormatFloat(scalePct*100.0, 'f', 4, 64) + "%"

	for i := 0; i < len(files); i++ {
		curFilePath := dir + files[i].Name()
		index := strings.LastIndex(files[i].Name(), "/") + 1
		newFilePath := scaledDir + files[i].Name()[index:]
		fmt.Println(curFilePath)
		fmt.Println(newFilePath)

		// cmd to scale image
		cmdResult := exec.Command("magick", curFilePath, "-resize", scalePctFormated, newFilePath)
		_, err := cmdResult.Output()
		if err != nil {
			fmt.Println("Error resizing image:", err)
		}
		// fmt.Println("output: ", output)
	}

	return getImageDirEntrys(scaledDir)
}
