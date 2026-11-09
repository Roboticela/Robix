#include "IconWeb.hpp"

void RoboWebAppendIconCss(std::ostream& css) {
  css << ".robo-icon{display:inline-flex;align-items:center;justify-content:center;color:var(--text-primary);flex:0 0 auto;line-height:0}\n";
  css << ".robo-icon svg{width:100%;height:100%;display:block}\n";
}

const char* RoboWebIconClass() { return "robo-icon"; }
