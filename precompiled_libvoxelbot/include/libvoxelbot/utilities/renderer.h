#pragma once
#include "SDL.h"

struct InfluenceMap;

struct MapRenderer {
  private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
  public:

    MapRenderer() {}
    MapRenderer(const char* title, int x, int y, int w, int h, unsigned int flags = 0);
    void shutdown();
    void renderInfluenceMap(const InfluenceMap& influenceMap, int x0, int y0);
    void renderInfluenceMapNormalized(const InfluenceMap& influenceMap, int x0, int y0);
    void renderMatrix1BPP(const char* bytes, int w_mat, int h_mat, int off_x, int off_y, int px_w, int px_h);
    void renderMatrix8BPPHeightMap(const char* bytes, int w_mat, int h_mat, int off_x, int off_y, int px_w, int px_h);
    void renderMatrix8BPPPlayers(const char* bytes, int w_mat, int h_mat, int off_x, int off_y, int px_w, int px_h);
    void renderImageRGB(const char* bytes, int width, int height, int off_x, int off_y, int scale);
    void renderImageGrayscale(const double* values, int width, int height, int off_x, int off_y, int scale, bool normalized);
    void present();
};
