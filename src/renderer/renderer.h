#ifndef OBRIS_RENDERER_H
#define OBRIS_RENDERER_H

#include "obris.h"
#include <cstdint>

namespace obris {

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(const ObrisConfig& config);
    void shutdown();
    void renderFrame();
    void resize(int w, int h);

private:
    bool initialized_ = false;
    int width_ = 720;
    int height_ = 1280;
};

} // namespace obris

#endif
