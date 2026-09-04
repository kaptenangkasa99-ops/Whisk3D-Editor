#ifndef WELCOME_H
#define WELCOME_H

#include "ViewPorts/ViewPorts.h"

class Welcome : public ViewportBase {
public:
    int ViewportKind() const { return 9; }
    void Render() W3D_OVERRIDE;
    void Resize(int newW, int newH) W3D_OVERRIDE;
    bool Click(int mx, int my);
};

#endif // WELCOME_H
