#include "ViewPorts/Welcome.h"
#include "ViewPorts/LayoutInput.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/core/UI.h"
#include "objects/Textures.h"
#include "w3dGraphics.h"
#include "render/OpcionesRender.h"
#include <algorithm>

namespace gfx = w3dEngine;

static bool Dentro(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

struct WelcomeLayout {
    int left, top, width, height, gap;
};

static WelcomeLayout Medir(int width, int height) {
    WelcomeLayout r;
    
    int targetWidth = static_cast<int>(width * 0.70f);
    r.width = (std::max)(160, (std::min)(targetWidth, 400));
    
    if (r.width > width - marginGS * 2) {
        r.width = (std::max)(120, width - marginGS * 2);
    }

    r.height = UIBotonAltura();
    if (r.height < RenglonHeightGS) r.height = RenglonHeightGS;
    
    r.gap = (std::max)(marginGS, (std::min)(marginGS * 2, height / 25));

    int totalH = LetterHeightGS + r.gap + (r.height * 2) + r.gap;
    r.left = (width - r.width) / 2;
    r.top = (height - totalH) / 2;

    if (r.top < marginGS) {
        r.top = marginGS;
    }

    return r;
}

void Welcome::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
}

void Welcome::Render() {
    int glY = W3dPantallaAlto - y - height;
    const float* bg = ListaColores[static_cast<int>(ColorID::background)];
    const float* text = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];

    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
    gfx::Viewport(x, glY, width, height);

    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();

    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Texture2D);
    gfx::EnableArray(gfx::VertexArray);

    WelcomeLayout layout = Medir(width, height);

    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    float panel[4] = { 0.10f, 0.13f, 0.16f, 1.0f };
    float quad[12];

    for (int n = 0; n < 2; n++) {
        int by = layout.top + LetterHeightGS + layout.gap + n * (layout.height + layout.gap);
        const float* color = (n == 0) ? accent : panel;
        gfx::Color4fv(color);

        quad[0] = (float)layout.left;                quad[1] = (float)by;
        quad[2] = (float)(layout.left + layout.width); quad[3] = (float)by;
        quad[4] = (float)(layout.left + layout.width); quad[5] = (float)(by + layout.height);
        
        quad[6] = (float)layout.left;                quad[7] = (float)by;
        quad[8] = (float)(layout.left + layout.width); quad[9] = (float)(by + layout.height);
        quad[10] = (float)layout.left;               quad[11] = (float)(by + layout.height);

        gfx::VertexPointer2f(0, quad); 
        gfx::DrawTrianglesArray(6);
    }

    gfx::BindTexture(Textures[0]->iID);
    gfx::Enable(gfx::Texture2D); 
    gfx::Enable(gfx::Blend);
    gfx::Color4fv(text);

    gfx::PushMatrix(); 
    gfx::Translatef((GLfloat)(width / 2), (GLfloat)layout.top, 0);
    RenderBitmapText("Welcome to Whisk3D", textAlign::center, (std::min)(width - marginGS * 2, 400)); 
    gfx::PopMatrix();

    for (int n = 0; n < 2; n++) {
        int by = layout.top + LetterHeightGS + layout.gap + n * (layout.height + layout.gap);
        gfx::Color4fv(n == 0 ? ListaColores[static_cast<int>(ColorID::negro)] : text);
        
        gfx::PushMatrix(); 
        gfx::Translatef((GLfloat)(width / 2), (GLfloat)(by + (layout.height - LetterHeightGS) / 2), 0);
        RenderBitmapText(n == 0 ? "New Project" : "Open Project", textAlign::center, layout.width); 
        gfx::PopMatrix();
    }

    gfx::Disable(gfx::ScissorTest);
}

bool Welcome::Click(int mx, int my) {
    WelcomeLayout layout = Medir(width, height);

    int b1_y = layout.top + LetterHeightGS + layout.gap;
    if (Dentro(mx - x, my - y, layout.left, b1_y, layout.width, layout.height)) {
        LayoutBienvenidaNuevoProyecto();
        return true;
    }

    int b2_y = b1_y + layout.height + layout.gap;
    if (Dentro(mx - x, my - y, layout.left, b2_y, layout.width, layout.height)) {
        LayoutBienvenidaAbrirProyecto();
        return true;
    }

    return false;
}