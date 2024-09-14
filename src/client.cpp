#include <manager/DASHManager.h>
#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <chrono>
#include <fstream>
#include <algorithm>  // For min_element

using namespace dash::network;
using namespace dash::mpd;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class DASHClient {
public:
    DASHClient(const std::string& url) {
        std::string mpdContent = fetchMPDFile(url);  // Step 1: Fetch MPD file

        if (mpdContent.empty()) {
            std::cerr << "Failed to fetch MPD from: " << url << std::endl;
            exit(EXIT_FAILURE);
        }

        // Step 2: Parse MPD content using DASHManager
        this->manager = new dash::DASHManager();
        this->mpd = manager->Open((char*)url.c_str());

        if (mpd == nullptr) {
            std::cerr << "Failed to parse MPD content from: " << url << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    ~DASHClient() {
        if (manager)
            delete manager;
    }

    void downloadSegments() {
        IPeriod* period = this->mpd->GetPeriods().at(0);
        IAdaptationSet* videoAdaptationSet = period->GetAdaptationSets().at(0);
        std::vector<IRepresentation*> representations = videoAdaptationSet->GetRepresentation();

        uint32_t segmentNumber = representations[0]->GetSegmentTemplate()->GetStartNumber();
        uint32_t maxSegments = 10;  // Number of segments to download (for testing)

        // For each segment, choose the appropriate representation and download
        for (uint32_t i = 0; i < maxSegments; ++i) {
            std::cout << "Downloading segment " << i << std::endl;

            IRepresentation* bestRepresentation = selectRepresentation(representations);
            downloadAndSaveSegment(bestRepresentation, segmentNumber);
            segmentNumber++;
        }
    }

private:
    dash::IDASHManager* manager;
    IMPD* mpd;
    double currentBandwidthKbps = 5000.0;  // Initial assumed bandwidth (in Kbps)

    // Step 1: Fetch MPD file via HTTP request
    std::string fetchMPDFile(const std::string& mpdURL) {
        CURL* curl;
        CURLcode res;
        std::string mpdContent;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, mpdURL.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mpdContent);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "Failed to fetch MPD: " << curl_easy_strerror(res) << std::endl;
            }

            curl_easy_cleanup(curl);
        }

        std::cout << "Fetched MPD content from: " << mpdURL << std::endl;
        std::cout << "MPD content: " << mpdContent << std::endl;
        return mpdContent;
    }

    std::string downloadSegment(const std::string& segmentURL) {
        std::cout << "Starting donwload of segment: " << segmentURL << std::endl;

        CURL* curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, segmentURL.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            auto start = std::chrono::high_resolution_clock::now();
            res = curl_easy_perform(curl);
            auto end = std::chrono::high_resolution_clock::now();

            if (res != CURLE_OK) {
                std::cerr << "Failed to download segment: " << curl_easy_strerror(res) << std::endl;
            }

            curl_easy_cleanup(curl);

            // Measure download time
            std::chrono::duration<double> duration = end - start;
            double timeTakenSec = duration.count();
            double segmentSizeKB = readBuffer.size() / 1024.0;
            std::cout << "Downloaded segment size: " << segmentSizeKB << " KB" << std::endl;

            // Calculate bandwidth (in Kbps)
            double bandwidthKbps = (segmentSizeKB / timeTakenSec) * 8.0;
            updateBandwidth(bandwidthKbps);
        }

        return readBuffer;
    }

    void updateBandwidth(double bandwidthKbps) {
        currentBandwidthKbps = (currentBandwidthKbps * 0.8) + (bandwidthKbps * 0.2);
        std::cout << "Updated bandwidth estimate: " << currentBandwidthKbps << " Kbps" << std::endl;
    }

    IRepresentation* selectRepresentation(const std::vector<IRepresentation*>& representations) {
        IRepresentation* bestRepresentation = nullptr;

        for (auto& representation : representations) {
            if (representation->GetBandwidth() <= currentBandwidthKbps * 1000) {
                bestRepresentation = representation;
            }
        }

        if (bestRepresentation == nullptr) {
            bestRepresentation = *min_element(representations.begin(), representations.end(),
                                              [](IRepresentation* a, IRepresentation* b) {
                                                  return a->GetBandwidth() < b->GetBandwidth();
                                              });
        }

        std::cout << "Selected representation with bitrate: " << bestRepresentation->GetBandwidth() << " bps" << std::endl;
        return bestRepresentation;
    }

    // Helper function to replace placeholders in the template string
    std::string replacePlaceholder(const std::string& templateStr, const std::string& placeholder, const std::string& value) {
        std::string result = templateStr;
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
        return result;
    }

    void downloadAndSaveSegment(IRepresentation* representation, uint32_t segmentNumber) {
        ISegmentTemplate* segmentTemplate = representation->GetSegmentTemplate();
        std::string baseURL;

        if (representation->GetBaseURLs().empty()) {
            // Try adaptation set's base URL
            if (!representation->GetBaseURLs().empty()) {
                baseURL = representation->GetBaseURLs().at(0)->GetUrl();
            } else {
                baseURL = "http://localhost:8080";  // Or use the URL of the MPD
            }
        } else {
            baseURL = representation->GetBaseURLs().at(0)->GetUrl();
        }

        std::string mediaTemplate = segmentTemplate->Getmedia();
        std::string representationID = representation->GetId();
        std::string mediaUrl = mediaTemplate;

        // Replace placeholders
        mediaUrl = replacePlaceholder(mediaUrl, "$RepresentationID$", representationID);
        mediaUrl = replacePlaceholder(mediaUrl, "$Number$", std::to_string(segmentNumber));

        std::string segmentURL = baseURL + "/" + mediaUrl;

        std::cout << "Downloading segment: " << segmentURL << std::endl;
        std::string segmentData = downloadSegment(segmentURL);

        if(segmentData.length() <= 50) {    //Somehow failed, html error message
            std::cerr << "Failed to download segment: " << segmentURL << std::endl;
            return;
        }

        saveSegmentToFile(segmentNumber, segmentData, mediaUrl);
    }
    void saveSegmentToFile(uint32_t segmentNumber, const std::string& segmentData, std::string savename) {
        std::ofstream outFile(savename, std::ios::binary);
        outFile.write(segmentData.c_str(), segmentData.size());
        outFile.close();
        std::cout << "Segment " << segmentNumber << " saved to " << savename << std::endl;
    }
};

int main(int argc, char** argv) {
    std::string mpdURL = "http://localhost:8080/kinetics2.mpd";
    DASHClient client(mpdURL);
    client.downloadSegments();

    return EXIT_SUCCESS;
}
