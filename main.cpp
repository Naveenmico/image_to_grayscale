#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <string>
#include <cstring>
#include <dirent.h>
#include <chrono> // For std::this_thread::sleep_for

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void rgbToGrayscale(const unsigned char* rgbImage, unsigned char* grayscaleImage, int width, int height) {
    for (int i = 0; i < width * height; ++i) {
        grayscaleImage[i] = static_cast<unsigned char>(
            0.2989 * rgbImage[3 * i] + 0.5870 * rgbImage[3 * i + 1] + 0.1140 * rgbImage[3 * i + 2]
        );
    }
}

struct Image {
    int width;
    int height;
    unsigned char* address;

    Image(int w, int h, unsigned char* addr) : width(w), height(h), address(addr) {}
};

std::mutex q1_mutex, q2_mutex;
std::queue<Image*> q1, q2;

bool isProcessing = true;

void readImages(const std::string& folderPath) {
    DIR* dir;
    struct dirent* ent;

    if ((dir = opendir(folderPath.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr && isProcessing) {
            if (ent->d_type == DT_REG) {
                std::string imagePath = folderPath + '/' + ent->d_name;

                int width, height, channels;
                unsigned char* rgbImage = stbi_load(imagePath.c_str(), &width, &height, &channels, 3);

                if (!rgbImage) {
                    std::cerr << "Error loading image " << imagePath << ": " << stbi_failure_reason() << std::endl;
                    continue;
                }

                Image* img = new Image(width, height, rgbImage);

                {
                    std::lock_guard<std::mutex> lock(q1_mutex);
                    q1.push(img);
                }
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error opening directory: " << folderPath << std::endl;
    }

    isProcessing = false; // Signal that no more images will be pushed to q1
}
// ... (previous code)
// ... (previous code)

void convertToGrayscale() {
    while (true) {
        Image* img = nullptr;

        {
            std::lock_guard<std::mutex> lock(q1_mutex);
            if (!q1.empty()) {
                img = q1.front();
                q1.pop();
            }
        }

        if (img) {
            // Process the image
            unsigned char* grayscaleImage = new unsigned char[img->width * img->height];
            rgbToGrayscale(img->address, grayscaleImage, img->width, img->height);

            {
                std::lock_guard<std::mutex> lock2(q2_mutex);
                q2.push(new Image(img->width, img->height, grayscaleImage));
                std::cout << "Thread 2: Image converted and added to q2\n";
            }

            // Deallocate memory used by the original RGB image
            stbi_image_free(img->address);
            delete img;
        } else {
            std::this_thread::yield();

            // Check if processing is complete
            if (!isProcessing && q1.empty()) {
                break;
            }
        }
    }
}


void saveToHardDisk() {
    static int counter = 0;  // Separate counter for generating unique filenames

    while (true) {
        Image* img = nullptr;

        {
            std::lock_guard<std::mutex> lock(q2_mutex);
            if (!q2.empty()) {
                img = q2.front();
                q2.pop();
            }
        }

        if (img) {
            // Save the image to hard disk
            std::string outputFolderPath = "/home/naveend/Assignment/image_processing/output/";
            std::string outputImagePath = outputFolderPath + "grayscale_image" + std::to_string(counter++) + ".png";

            if (!stbi_write_png(outputImagePath.c_str(), img->width, img->height, 1, img->address, img->width)) {
                std::cerr << "Thread 4: Error writing grayscale image: " << stbi_failure_reason() << std::endl;
            } else {
                std::cout << "Thread 4: Grayscale image saved to: " << outputImagePath << std::endl;
            }

            // Deallocate memory used by the grayscale image
            delete[] img->address;
            delete img;
        } else {
            std::this_thread::yield();

            // Check if processing is complete
            if (!isProcessing && q2.empty()) {
                break;
            }
        }
    }
}


// ... (main function remains the same)


int main() {
    std::string folderPath = "/home/naveend/Assignment/image_processing/images";

    // Start threads to process images
    std::thread t1(readImages, folderPath);
    std::thread t2(convertToGrayscale);
    std::thread t4(saveToHardDisk);

    // Join threads to the main thread
    t1.join();
    t2.join();
    t4.join();

    return 0;
}
