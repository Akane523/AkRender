#pragma once

namespace AkRender {

    class RenderImpl;

    class Render {
        public:
            Render();
            ~Render();
        private:
            RenderImpl* impl;
    };
}