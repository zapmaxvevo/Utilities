#include "App.h"
#include "MainFrame.h"

wxIMPLEMENT_APP(App);

bool App::OnInit()
{
    if (!wxApp::OnInit()) return false;

    auto* frame = new MainFrame();
    frame->Show();
    return true;
}
