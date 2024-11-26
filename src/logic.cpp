#include "cpp/poppler-rectangle.h"
#include "poppler.hpp"
#include "opencv.hpp"
#include "ptv.hpp"
#include "logic.hpp"
#include <algorithm>
#include <cstddef>
#include <opencv2/core.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/types.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <utility>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <ctime>

using std::string;
using std::vector;
namespace fs = std::filesystem;

void scale_images_to_width(vector<cv::Mat> &images, int dst_width) {
    for (int i = 0; i < (int)images.size(); i++) {
        cv::Mat new_img;
        double scale = (double)dst_width / (double)images[i].cols;
        cv::resize(images[i], new_img, cv::Size(), scale, scale, cv::INTER_LINEAR);
        images[i] = new_img;
    }
}

void scale_images_to_height(vector<cv::Mat> &images, int dst_height) {
    for (int i = 0; i < (int)images.size(); i++) {
        cv::Mat new_img;
        double scale = (double)dst_height / (double)images[i].cols;
        cv::resize(images[i], new_img, cv::Size(), scale, scale, cv::INTER_LINEAR);
        images[i] = new_img;
    }
}

void scale_images_to_fit(vector<cv::Mat> &images, struct VP &vp) {
    for (int i = 0; i < (int)images.size(); i++) {
        cv::Mat img(images[i]);
        double scale_w;
        double scale_h;
        double scale = 1.0;

        if (img.cols > vp.width && img.cols > vp.height) {
            scale_w = (double)vp.width / (double)images[i].cols;
            scale_h = (double)vp.height / (double)images[i].rows;
            scale = std::min(scale_w, scale_h);
        }
        if (img.cols < vp.width && img.rows < vp.height) {
            scale_w = (double)vp.width / (double)images[i].cols;
            scale_h = (double)vp.height / (double)images[i].rows;
            scale = std::min(scale_w, scale_h);
        }
        if (img.cols > vp.width && img.rows <= vp.height) {
            scale_w = (double)vp.width / (double)img.cols;
            scale = scale_w;
        }
        if (img.cols <= vp.width && img.rows > vp.height) {
            scale_h = (double)vp.height / (double)img.rows;
            scale = scale_h;
        }

        cv::Mat new_img;
        cv::resize(images[i], new_img, cv::Size(), scale, scale, cv::INTER_LINEAR);
        images[i] = new_img;
    }
}

void make_frames_dir(string frames_dir) {
    if (fs::exists(frames_dir)) {
        std::cout << "Frames directory already exists." << std::endl;
    }
    else {
        fs::create_directory(frames_dir);
        std::cout << "Created frames directory." << std::endl;
    }
}

void make_pdf_dir(string pdf_dir) {
    if (!fs::exists(pdf_dir)) {
        fs::create_directory(pdf_dir);
        std::cout << "Created PDF directory." << std::endl;
    } else {
        std::cout << "PDF directory already exists." << std::endl;
    }
}

string format_path(string str) {
    string bad_chars = " ()&-<>";
    for (int i = 0; i < (int)str.length(); i++) {
        if ((int)bad_chars.find(str[i]) > -1) {
            str[i] = '_';
        }
    }
    return str;
}

void generate_video(string frames_dir, string output, struct VP &vp) {
    while (fs::exists(output)) {
        output.insert((output.length() - 4), 1, '+');
    }

    vector<string> cmd_args = {
        "-framerate",
        std::to_string(vp.fps),
        "-i",
        frames_dir + "%06d.jpg",
        "-s",
        std::to_string(vp.width) + "x" + std::to_string(vp.height),
        output
    };

    string cmd = "ffmpeg";
    for (string arg : cmd_args) {
        cmd += " " + arg;
    }

    std::cout << "FFmpeg CMD: " << cmd << std::endl;

    std::cout << "Generating video..." << std::endl;
    std::system(cmd.c_str());

    std::cout << "Output path: " << output << std::endl;
}

void generate_scroll_frames(string frames_dir, int pages, cv::Mat long_image, struct VP &vp) {
    int pixels_translated = (double)long_image.rows / (vp.fps * vp.spp * pages);
    if (pixels_translated == 0) {
        pixels_translated = 1;
    }
    int height = long_image.rows - vp.height; // - vp.rows prevents out of bounds error with cv::Rect roi
    cv::Mat tmp_img(vp.height, vp.width, CV_8UC4);

    std::cout << "Pixels per frame: " << pixels_translated << std::endl;

    for (int h = 0, i = 0; h < height; h += pixels_translated, i++) {
        cv::Rect roi = cv::Rect(0, h, vp.width, vp.height);
        long_image(roi).copyTo(tmp_img);
        string file = frames_dir + get_frame_name(i) + ".jpg";
        cv::imwrite(file, tmp_img, {cv::IMWRITE_JPEG_QUALITY, 90});
    }
}

void generate_sequence_frames(string frames_dir, vector<cv::Mat> imgs, struct VP &vp) {
    int count = 0;
    cv::Mat vp_img;

    for (size_t i = 0; i < imgs.size(); i ++) {
        vp_img = cv::Mat(vp.height, vp.width, CV_8UC4, cv::Scalar(0, 0, 0));
        int x = 0;
        int y = 0;

        // adds offset
        if (vp_img.cols - imgs[i].cols >= 2) {
            x += (vp_img.cols - imgs[i].cols) / 2;
        } else if (vp_img.rows - imgs[i].rows >= 2) {
            y += (vp_img.rows - imgs[i].rows) / 2;
        }

        // prevents stretching of images when being rendered.
        // keeps them within the vp.
        cv::Rect2i roi(x, y, imgs[i].cols, imgs[i].rows);
        imgs[i].copyTo(vp_img(roi));

        string path = frames_dir + get_frame_name(count) + ".jpg";
        cv::imwrite(path, vp_img, {cv::IMWRITE_JPEG_QUALITY, 90});
        count++;
    }
}

string get_frames_dir(string path) {
    std::srand(std::time(0));
    int rand_num = std::rand() % 999999 + 100000;
    string result;
    string sub_dir;

    int end = path.length() - 1;
    for (int i = path.length() - 2; i > -1; i--) {
        if (path[i] == '/') {
            end = i + 1;
            break;
        }
    }

    sub_dir = path.substr(0, end);
    result = sub_dir + "ptv-frames-" + std::to_string(rand_num) + "/";
    return result;
}

vector<cv::Mat> get_images(string dir) {
    vector<cv::Mat> images;
    std::map<int, string> image_map;
    // creates hash-map of valid image paths in numerical order
    for (const auto &entry : fs::directory_iterator(dir)) {
        string path;
        int index;
        if (!fs::is_directory(entry)) {
            path = entry.path().string();
            string name = entry.path().filename().string();
            try {
                index = std::stoi(name.substr(0, name.length() - name.find_last_of('.')));
            } catch (const std::invalid_argument& e) {
                std::cerr << "<!> Invalid Argument: " << name << " contains no int so is not used." << std::endl;
            } catch (const std::out_of_range& e) {
                std::cerr << "<!> Out of Range: geting int value from image name, get_images()." << std::endl;
            }
            image_map.insert(std::make_pair(index, path));
        }
    }
    // reads images in numerical order
    for (size_t i = 0; i < image_map.size(); i++) {
        if (image_map[i] == "") continue;

        cv::Mat mat = cv::imread(image_map[i]);

        // makes dimentions of the image divisible by 2.
        // ffmpeg will get upset if otherwise.
        int rows = mat.rows % 2 != 0 ? mat.rows + 1 : mat.rows;
        int cols = mat.cols % 2 != 0 ? mat.cols + 1 : mat.cols;

        cv::Rect2i roi(0, 0, mat.cols, mat.rows);
        cv::Mat tmp_mat(rows, cols, CV_8UC3);
        mat.copyTo(tmp_mat(roi));

        images.push_back(cv::Mat(tmp_mat));
    }
    return images;
}

vector<cv::Mat> get_images_new(poppler::document *pdf, Style style, VP &vp) {
    int pages = pdf->pages();
    auto renderer = poppler::page_renderer();
    vector<cv::Mat> images = {};

    // required to copy pdf data to vector
    for (int i = 0; i < pages; i++) {
        images.emplace_back(cv::Mat(100, 100, CV_8UC1, cv::Scalar(100, 210, 70)));
    }

    for (int i = 0; i < pages; i++) {
        int fmt = CV_8UC3;
        double dpi = DEFAULT_DPI;
        poppler::page *page = pdf->create_page(i);

        // scales pages to correctly fit inside displayport.
        if (style == Style::SCROLL) {
            dpi = get_scaled_dpi_from_width(page, vp.width);
        } else if (style == Style::SEQUENCE) {
            dpi = get_scaled_dpi(page, vp);
        }

        poppler::image img = renderer.render_page(page, dpi, dpi);
        cv::Mat mat;

        // Determine the format
        if (img.data() == nullptr) {
            std::cerr << "<!> Error: Page " << i << " has no data to load." << std::endl;
        } else if (img.format() == poppler::image::format_invalid) {
            std::cerr << "<!> Error: Page " << i << " has invalid image format." << std::endl;
        } else if (img.format() == poppler::image::format_gray8) {
            fmt = CV_8UC1;
            mat = cv::Mat(img.height(), img.width(), fmt, img.data(), img.bytes_per_row());
        } else if (img.format() == poppler::image::format_rgb24) {
            fmt = CV_8UC3;
            mat = cv::Mat(img.height(), img.width(), fmt, img.data(), img.bytes_per_row());
        } else if (img.format() == poppler::image::format_bgr24) {
            fmt = CV_8UC3;
            mat = cv::Mat(img.height(), img.width(), fmt, img.data(), img.bytes_per_row());
        } else if (img.format() == poppler::image::format_argb32) {
            fmt = CV_8UC4;
            mat = cv::Mat(img.height(), img.width(), fmt, img.data(), img.bytes_per_row());
        }

        // makes sure dimentions are even
        // ffmpeg will get upset if odd
        if (mat.rows % 2 != 0 || mat.cols % 2 != 0) {
            int rows = mat.rows % 2 != 0 ? mat.rows + 1 : mat.rows;
            int cols = mat.cols % 2 != 0 ? mat.cols + 1 : mat.cols;

            cv::Rect2i roi(0, 0, mat.cols, mat.rows);
            cv::Mat tmp_mat(rows, cols, fmt);
            mat.copyTo(tmp_mat(roi));

            tmp_mat.copyTo(images[i]);
        } else {
            mat.copyTo(images[i]);
        }
    }

    return images;
}

cv::Mat get_long_image(vector<cv::Mat> &imgs, struct VP &vp) {
    int height = 0;
    int type = imgs[0].type();
    cv::Mat long_image;

    // stores exported pdf pages as images in vector.
    for (size_t i = 0; i < imgs.size(); i++) {
        height += imgs[i].rows;
    }
    // adds white space before and after content.
    height += vp.height * 2;

    // images that will contain all contents of pdf (like a long comic strip)
    long_image = cv::Mat(height, vp.width, type);

    // sequentually adds all pages of pdf in order to long_image.
    // ROI -> Region of Interest
    for (size_t i = 0, h = vp.height; i < imgs.size(); i++) {
        int r = imgs[i].rows;
        cv::Rect roi = cv::Rect(0, h, long_image.cols, r);
        imgs[i].copyTo(long_image(roi));
        h += r;
    }

    return long_image;
}

// returns dpi to scale page to viewport width
double get_scaled_dpi_from_width(poppler::page *page, int width) {
    auto rect = page->page_rect(poppler::media_box);

    if (rect.width() == width) {
        return DEFAULT_DPI;
    }

    return ((double)width * DEFAULT_DPI) / (double)rect.width();
}

// returns dpi that will scale the pdf page to fit the viewport dimentions
double get_scaled_dpi(poppler::page *page, struct VP &vp) {
    double dpi_w;
    double dpi_h;
    poppler::rectf rect = page->page_rect(poppler::media_box);

    if (rect.width() > vp.width && rect.height() > vp.height) {
        dpi_w = ((double)vp.width * DEFAULT_DPI) / rect.width();
        dpi_h = ((double)vp.height * DEFAULT_DPI) / rect.height();
        return std::min(dpi_w, dpi_h);
    }
    if (rect.width() < vp.width && rect.height() < vp.height) {
        dpi_w = ((double)vp.width * DEFAULT_DPI) / rect.width();
        dpi_h = ((double)vp.height * DEFAULT_DPI) / rect.height();
        return std::min(dpi_w, dpi_h);
    }
    if (rect.width() > vp.width && rect.height() <= vp.height) {
        dpi_w = ((double)vp.width * DEFAULT_DPI) / rect.width();
        return dpi_w;
    }
    if (rect.width() <= vp.width && rect.height() > vp.height) {
        dpi_h = ((double)vp.height * DEFAULT_DPI) / rect.height();
        return dpi_h;
    }

    return DEFAULT_DPI;
}

string get_pdf_dir(string pdf_path) {
    string pdf_dir = pdf_path.substr(0, pdf_path.length() - 4) + "_" + pdf_path.substr(pdf_path.length() - 3) + "/";
    return pdf_dir;
}

string get_pdf_name(string pdf_path) {
    return pdf_path.substr(pdf_path.find_last_of("/") + 1, pdf_path.length() - 7);
}

// returns numerical name as string no file extension
string get_frame_name(int index) {
    if (index < 10) {
        return "00000" + std::to_string(index);
    } else if (index < 100) {
        return "0000" + std::to_string(index);
    } else if (index < 1000) {
        return "000" + std::to_string(index);
    } else if (index < 10000) {
        return "00" + std::to_string(index);
    } else if (index < 100000) {
        return "0" + std::to_string(index);
    } else if (index < 1000000) {
        return std::to_string(index);
    }
    return "z";
}

// returns numerical name as string no file extension
string get_page_name(int index, int pages) {
    if (pages < 10) {
        return std::to_string(index);
    } else if (pages < 100) {
        if (index < 10) {
            return "0" + std::to_string(index);
        } else {
            return std::to_string(index);
        }
    } else if (pages < 1000) {
        if (index < 10) {
            return "00" + std::to_string(index);
        } else if (index < 100) {
            return "0" + std::to_string(index);
        } else {
            return std::to_string(index);
        }
    } else if (pages < 10000) {
        if (index < 10) {
            return "000" + std::to_string(index);
        } else if (index < 100) {
            return "00" + std::to_string(index);
        } else if (index < 1000) {
            return "0" + std::to_string(index);
        } else {
            return std::to_string(index);
        }
    } else {
        if (index < 10) {
            return "0000" + std::to_string(index);
        } else if (index < 100) {
            return "000" + std::to_string(index);
        } else if (index < 1000) {
            return "00" + std::to_string(index);
        } else if (index < 10000) {
            return "0" + std::to_string(index);
        } else if (index < 100000) {
            return std::to_string(index);
        }
    }
    return "z";
}

bool delete_dir(string dir) {
    if (fs::is_directory(dir)) {
        return fs::remove_all(fs::path(dir));
    }
    std::cout << "<!> Error: cannot delete " << dir << "" << std::endl;
    return false;
}
