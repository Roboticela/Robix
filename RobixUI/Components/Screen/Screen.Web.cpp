#include "Screen.Web.hpp"

void RoboWebAppendScreenCss(std::ostream& css) {
  css << ".robo-window{position:absolute;z-index:1;box-sizing:border-box;color:var(--text-primary);"
         "background:var(--bg-secondary);border:0;outline:none;overflow:auto}\n";
  css << ".robo-screen{background:var(--bg-primary);color:var(--text-primary)}\n";
  css << ".robo-window:not(.is-open){display:none}\n";
  css << ".robo-page .robo-window,.robo-page .robo-window:not(.is-open){position:relative;display:flex;width:100%;height:auto;left:auto;top:auto;max-height:none;z-index:auto}\n";
}

const char* RoboWebScreenClass(bool root) { return root ? "robo-screen" : "robo-window"; }

const char* RoboWebScreenTag(bool root) { return root ? "div" : "section"; }
