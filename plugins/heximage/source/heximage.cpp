#include <iostream>
#include <cstdio>
#include <hex/plugin.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/helpers/LuaConfig.hpp>

#include <hex/helpers/paths.hpp>

#include <imgui_memory_editor.h>

namespace hex::view::heximage
{
    class BMP {
    public:
        // rewrite from https://www.cnblogs.com/ipersevere/p/12756048.html
        virtual unsigned char *Array2ImageArray(unsigned char *inputImg, int width, int height, int *ouputSize){

            /*create a bmp format file*/
            int bitmap_x = (int)ceil((double)width * 3 / 4) * 4;
            unsigned char *bitmap = (unsigned char*)malloc(sizeof(unsigned char)*height*bitmap_x + 54);

            bitmap[0] = 'B';
            bitmap[1] = 'M';
            write_bmpheader(bitmap, 2, 4, height*bitmap_x + 54); //whole file size
            write_bmpheader(bitmap, 0xA, 4, 54); //offset before bitmap raw data
            write_bmpheader(bitmap, 0xE, 4, 40); //length of bitmap info header
            write_bmpheader(bitmap, 0x12, 4, width); //width
            write_bmpheader(bitmap, 0x16, 4, height); //height
            write_bmpheader(bitmap, 0x1A, 2, 1);
            write_bmpheader(bitmap, 0x1C, 2, 24); //bit per pixel
            write_bmpheader(bitmap, 0x1E, 4, 0); //compression
            write_bmpheader(bitmap, 0x22, 4, height*bitmap_x); //size of bitmap raw data

            for (int i = 0x26; i < 0x36; i++)
                bitmap[i] = 0;

            int k = 54;
            for (int i = height - 1; i >= 0; i--) {
                int j;
                for (j = 0; j < width; j++) {
                    int index = i*width + j;
                    for (int l = 0; l < 3; l++)
                        bitmap[k++] = inputImg[index];
                }
                j *= 3;
                while (j < bitmap_x) {
                    bitmap[k++] = 0;
                    j++;
                }
            }

            *ouputSize = k;
            std::memcpy(inputImg, bitmap, k);
            free(bitmap);
            return bitmap;
        }
        virtual void Array2ImageFile(unsigned char* inputImg, int width, int height, char* outFilename){
            int size = 0;
            unsigned char *bmp = Array2ImageArray(inputImg, width, height, &size);
            FILE *fp = fopen(outFilename, "wb+");
            if (fp == NULL) {
                printf("Could not open file: %s", outFilename);
            }
            fwrite(bmp, 1, size, fp);
            fclose(fp);
            free(bmp);
        }
    private:
        void write_bmpheader(unsigned char *bitmap, int offset, int bytes, int value) {
            int i;
            for (i = 0; i < bytes; i++)
                bitmap[offset + i] = (value >> (i << 3)) & 0xFF;
        }
    };
    class ViewHexImage : public hex::View {
    public:
        ViewHexImage() : hex::View("HexImage") {

            EventManager::subscribe<EventDataChanged>(this, [this]() {
                this->m_shouldInvalidate = true;
            });

            EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
                if (this->m_shouldMatchSelection) {
                    if (region.address == size_t(-1)) {
                        this->m_hashRegion[0] = this->m_hashRegion[1] = 0;
                    } else {
                        this->m_hashRegion[0] = region.address;
                        this->m_hashRegion[1] = region.size + 1;    // WARNING: get size - 1 as region size
                    }
                    this->m_shouldInvalidate = true;
                }
            });
            
        }
        ~ViewHexImage() override { 
            EventManager::unsubscribe<EventDataChanged>(this);
            EventManager::unsubscribe<EventRegionSelected>(this);
        }

        void drawContent() override {
            
            if (ImGui::Begin("HexImage")) {
                auto provider = ImHexApi::Provider::get();
                if (ImHexApi::Provider::isValid() && provider->isAvailable()) {

                    ImGui::TextUnformatted("hex.builtin.common.region"_lang);
                    ImGui::Separator();

                    ImGui::InputScalarN("##nolabel", ImGuiDataType_U64, this->m_hashRegion, 2, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
                    if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                    ImGui::Checkbox("hex.builtin.common.match_selection"_lang, &this->m_shouldMatchSelection);
                    if (ImGui::IsItemEdited()) {
                        // Force execution of Region Selection Event
                        EventManager::post<RequestSelectionChange>(Region { 0, 0 });
                        this->m_shouldInvalidate = true;
                    }

                    size_t dataSize = provider->getSize();
                    if (this->m_hashRegion[1] >= provider->getBaseAddress() + dataSize)
                        this->m_hashRegion[1] = provider->getBaseAddress() + dataSize;

                    if (this->m_hashRegion[1] > N){
                        this->m_hashRegion[1] = N;
                    }
                    
                    provider->read(this->m_hashRegion[0], buffer.data(), this->m_hashRegion[1]);

                    

                    ImGui::SliderInt("Image width", &this->m_width, this->min_width, this->max_width);
                    int width = this->m_width;
                    int height = int(this->m_hashRegion[1]/width);


                    std::string output("");
                    output += "width:";
                    output += std::to_string(width) + std::string("\t");
                    output += "height:";
                    output += std::to_string(height);
                    ImGui::Text(output.c_str());


                    int ouputSize = 0;
                    image.Array2ImageArray(buffer.data(), width, height, &ouputSize);

                    // ImGui::Text("I am a fancy tooltip");
                    // static float arr[] = { 0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f };
                    // ImGui::PlotLines("Curve", arr, IM_ARRAYSIZE(arr));
                    

                    ImGui::Texture Texture_ID = ImGui::LoadImageFromMemory(reinterpret_cast<const ImU8 *>(buffer.data()), ouputSize);
                    if (Texture_ID == nullptr) {
                        log::fatal("Could not load image from bmp array!");
                        ImGui::Text("Could not load image from bmp array!");
                        exit(EXIT_FAILURE);
                    }

                    ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
                    ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
                    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
                    ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
                    ImGuiIO& io = ImGui::GetIO();
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::Image(Texture_ID, ImVec2(width, height), uv_min, uv_max, tint_col, border_col);
                    if (ImGui::IsItemHovered())
                    {
                        float my_tex_w = width;
                        float my_tex_h = height;
                        ImGui::BeginTooltip();
                        float region_sz = 16.0f;
                        float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
                        float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
                        float zoom = 8.0f;
                        if (region_x < 0.0f) { region_x = 0.0f; }
                        else if (region_x > my_tex_w - region_sz) { region_x = my_tex_w - region_sz; }
                        if (region_y < 0.0f) { region_y = 0.0f; }
                        else if (region_y > my_tex_h - region_sz) { region_y = my_tex_h - region_sz; }
                        ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
                        ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);
                        ImVec2 uv0 = ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
                        ImVec2 uv1 = ImVec2((region_x + region_sz) / my_tex_w, (region_y + region_sz) / my_tex_h);
                        ImGui::Image(Texture_ID, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, tint_col, border_col);
                        
                        region_x = io.MousePos.x - pos.x;
                        region_y = io.MousePos.y - pos.y;
                        ImGui::Text("image: (%.2f, %.2f)", region_x, region_y);

                        u64 current_data_start = region_y * width + region_x;
                        ImGui::Text("data start (%d)", current_data_start);
                        
                        ImGui::EndTooltip();                        

                        EventManager::post<RequestSelectionChange>(Region { current_data_start, 1024*1024 });
                    }

                }

            }
            ImGui::End();    
        }
    private:

        bool m_shouldInvalidate     = true;
        u64 m_hashRegion[2]         = { 0 };
        bool m_shouldMatchSelection = true;
        int m_width                 = 128;
        const int min_width         = 16;
        const int max_width         = 1024;
        const static int N = (1024 * 1024 * 2 + 54); //buffer + image header 
        std::array<u8, N> buffer      =   {0}; //image buffer            
        hex::view::heximage::BMP image;
        static MemoryEditor mem_edit_1;
    };
    IMHEX_PLUGIN_SETUP("HexImage", "dianwoshishi", "Convert Hex to Image!") {
        hex::ContentRegistry::Views::add<ViewHexImage>();
    }
} // namespace hex::view::heximage



