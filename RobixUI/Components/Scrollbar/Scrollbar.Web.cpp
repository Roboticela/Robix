#include "Scrollbar.Web.hpp"

const char* RoboWebScrollbarClass() { return "robo-scrollbar"; }

void RoboWebAppendScrollbarCss(std::ostream& css) {
  css << ".robo-scrollbar{position:relative;display:flex;flex-direction:column;align-items:flex-end;gap:3px;"
         "box-sizing:border-box;width:16px;min-height:28px;z-index:4;opacity:1}\n";
  css << ".robo-scrollbar[data-autohide=\"true\"]{opacity:1}\n";
  css << ".robo-scrollbar[data-arrows=\"off\"] .robo-scroll-arrow{display:none}\n";
  css << ".robo-scroll-arrow{width:6px;height:6px;flex:0 0 6px;border:0;padding:0;background:transparent;"
         "position:relative;cursor:pointer;outline:0}\n";
  css << ".robo-scroll-arrow::before{content:\"\";position:absolute;left:0;border-left:3px solid transparent;"
         "border-right:3px solid transparent}\n";
  css << ".robo-scroll-arrow.is-up::before{top:0;border-bottom:5px solid var(--text-tertiary)}\n";
  css << ".robo-scroll-arrow.is-down::before{top:1px;border-top:5px solid var(--text-tertiary)}\n";
  css << ".robo-scroll-track{position:relative;flex:1;width:6px;border-radius:980px;background:var(--border-default)}\n";
  css << ".robo-scroll-thumb{position:absolute;left:0;top:0;width:6px;height:40%;min-height:28px;border-radius:980px;"
         "background:var(--text-tertiary)}\n";
  css << ".robo-scrollbar.is-x{flex-direction:row;align-items:flex-end;width:auto;height:16px;min-width:28px;"
         "min-height:0}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-track{width:auto;height:6px;flex:1}\n";
  css << ".robo-scrollbar.is-x .robo-scroll-thumb{top:0;height:6px;width:40%;min-height:0;min-width:28px}\n";
}
