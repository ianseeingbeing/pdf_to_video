#include "cpp/poppler-global.h"
#include "poppler.hpp"
#include "opencv.hpp"
#include "logic.hpp"
#include "ptv.hpp"
#include <cctype>
#include <filesystem>
#include <libavformat/avformat.h>
#include <opencv2/imgcodecs.hpp>

using std::string;
// using std::vector;
namespace fs = std::filesystem;

int main(int argc, char **argv) {
    int width = 1280;
    int height = 720;
    double fps = 1.0;
    double spp = 1.0;
    string vid_fmt = "MP4";
    Style style = Style::SEQUENCE;
    string pdf_path = "";
    string img_seq_dir = "";
    bool keep = false;

    if (argc < 2) {
        std::cerr << HELP_TXT << std::endl;
        return 1;
    }

    // CLI
    for (int i = 1; i < argc; i++) {
        string arg = string(argv[i]);
        if (arg == "-h" || arg == "--help") {
            std::cout << HELP_TXT << std::endl;
            return 0;
        } else if ((int)arg.find(".pdf") > -1) {
            if (img_seq_dir != "") {
                std::cerr << "<!> Error: Cannot convert Image Sequence and PDF at the same time." << std::endl;
                return 1;
            }
            if (!fs::exists(arg)) {
                std::cerr << "<!> Error: PDF path does not exist." << std::endl;
                return 1;
            }
            if (arg.substr(arg.length() - 4, 4) != ".pdf") {
                std::cout << "<!> Error: PDF path not a .pdf file" << std::endl;
                return 1;
            }
            pdf_path = arg;
        } else if ((int)arg.find('/') > -1) {
            if (pdf_path != "") {
                std::cerr << "<!> Error: Cannot convert PDF and Image Sequence at the same time." << std::endl;
                return 1;
            }
            if (!fs::is_directory(arg)) {
                std::cerr << "<!> Error: directory path dosen't exist. Try again." << std::endl;
                return 1;
            }
            if (arg[arg.length() - 1] != '/') {
                arg.push_back('/');
            }
            img_seq_dir = arg;
        } else if (arg == "-r") {
            i++;
            width = std::stoi(argv[i]);
            i++;
            height = std::stoi(argv[i]);
        } else if (arg == "-f") {
            i++;
            fps = std::stof(argv[i]);
        } else if (arg == "-s") {
            i++;
            spp = std::stof(argv[i]);
        } else if (arg == "--format") {
            i++;
            vid_fmt = argv[i];
            if (vid_fmt != "MP4" && vid_fmt != "AVI" && vid_fmt != "MKV" && vid_fmt != "MOV") {
                std::cerr << "<!> Error: Video format not currently supported." << std::endl;
                return 1;
            }
        } else if (arg == "--scroll") {
            style = Style::SCROLL;
        } else if (arg == "--keep") {
            keep = true;
        } else {
            std::cerr << "<!> Error: unknown argument detected: " << argv[i] << std::endl;
            return 1;
        }
    }

    // pdf_loading_test();
    // return 1;

    // Info presented before continuing.
    if (pdf_path != "") {
        std::cout << "PDF Path: " << pdf_path << std::endl;
    }
    if (img_seq_dir != "") {
        std::cout << "Image Sequence Dir: " << img_seq_dir << std::endl;
    }
    std::cout << "Width: " << width << "\nHeight: " << height << "" << std::endl;
    std::cout << "FPS: " << fps << std::endl;
    std::cout << "SPP: " << spp << std::endl;
    std::cout << "Format: " << vid_fmt << std::endl;
    if (style == Style::SCROLL) {
        std::cout << "Style: SCROLL" << std::endl;
    } else if (style == Style::SEQUENCE) {
        std::cout << "Style: SEQUENCE" << std::endl;
    }

    // Continuing logic
    string check;
    std::cout << "Are these values correct: [Y/n]" << std::endl;
    std::getline(std::cin, check);
    if (!check.empty() && check != "Y" && check != "y") {
        return 1;
    }
    std::cin.clear();

    // to work with ffmpeg cli, needs lowercase fmt
    for (auto &c : vid_fmt) {
        c = std::tolower(c);
    }

    // creates the vp
    struct VP vp = {
        width,
        height,
        fps,
        spp,
        vid_fmt
    };

    // Application Logic
    // converting pdf to video
    if (pdf_path != "") {
        poppler::document *pdf = poppler::document::load_from_file(pdf_path);
        string frames_dir = get_frames_dir(pdf_path);

        // sets viewport's resolution to full res of first pdf page
        poppler::rectf rect = pdf->create_page(0)->page_rect(poppler::media_box);
        if (width == -1) {
            vp.width = rect.width();
        }
        if (height == -1) {
            vp.height = rect.height();
        }

        std::cout << "Loading pdf pages..." << std::endl;
        std::vector<cv::Mat> images = get_images(pdf, style, vp);

        make_frames_dir(frames_dir);

        if (style == Style::SCROLL) {
            cv::Mat long_img = get_long_image(images, vp);
            std::cout << "Generating video frames..." << std::endl;
            generate_scroll_frames(frames_dir, pdf->pages(), long_img, vp);
        } else if (style == Style::SEQUENCE) {
            std::cout << "Generating video frames..." << std::endl;
            generate_sequence_frames(frames_dir, images, vp);
        }

        string fmt_path = format_path(pdf_path);
        string output = fmt_path.substr(0, fmt_path.length() - 4) + "." + vp.fmt;
        generate_video(frames_dir, output, vp);

        if (!keep) {
            delete_dir(frames_dir);
        }

        std::cout << "Finished!" << std::endl;
    }

    // converting image sequence to video
    if (img_seq_dir != "") {
        string frames_dir = get_frames_dir(img_seq_dir);
        std::map<int, string> img_map = get_image_sequence_map(img_seq_dir);

        // sets viewport res to resolution of img
        cv::Mat img = cv::imread(img_map[-1]);
        if (width == -1) {
            vp.width = img.cols % 2 == 0 ? img.cols : img.cols + 1;
        }
        if (height == -1) {
            vp.height = img.rows % 2 == 0 ? img.rows : img.rows + 1;
        }
        img_map.erase(-1);

        std::cout << "Loading images..." << std::endl;
        vector<cv::Mat> images = get_images(img_map, style, vp);

        make_frames_dir(frames_dir);

        if (style == Style::SCROLL) {
            cv::Mat long_img = get_long_image(images, vp);
            std::cout << "Generating video frames..." << std::endl;
            generate_scroll_frames(frames_dir, images.size(), long_img, vp);
        } else if (style == Style::SEQUENCE) {
            std::cout << "Generating video frames..." << std::endl;
            generate_sequence_frames(frames_dir, images, vp);
        }

        string fmt_dir = format_path(img_seq_dir);
        string output = fmt_dir.substr(0, fmt_dir.length() - 1) + "." + vp.fmt;
        generate_video(frames_dir, output, vp);

        if (!keep) {
            delete_dir(frames_dir);
        }

        std::cout << "Finished!" << std::endl;
    }

    return 0;
}
