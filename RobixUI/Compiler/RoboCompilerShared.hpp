#pragma once

#include "RoboCompiler.hpp"
#include "RoboXML.hpp"

#include "../RoboShared/RoboTypes.hpp"

#include <initializer_list>
#include <string>

void RoboCompilerAddDiag(RoboCompilerResult& result, RoboCompilerDiagnostic::Severity severity, const std::string& path,
                         const std::string& message);
std::string RoboCompilerLower(std::string value);
bool RoboCompilerIsLayoutKey(const std::string& key);
bool RoboCompilerIsSharedStyleKey(const std::string& key);
bool RoboCompilerIsPaneFamily(const std::string& name);
bool RoboCompilerIsOneOf(const std::string& value, std::initializer_list<const char*> options);
bool RoboCompilerCheckOverflowAttrs(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
void RoboCompilerNoteUsedComponent(const std::string& name);
void RoboCompilerNoteUsedIcon(const std::string& iconName);
std::string RoboCompilerCppEscape(const std::string& value);
std::string RoboCompilerTrim(std::string value);
std::string RoboCompilerEmitLayoutProps(const RoboXMLNode& node, const std::string& componentExpr);
bool RoboCompilerCheckNode(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
std::string RoboCompilerEmitChildCpp(const RoboXMLNode& node, const std::string& parentExpr);
bool RoboCompilerIsIdent(const std::string& value);
std::string RoboCompilerIconNameFromNode(const RoboXMLNode& node);
bool RoboCompilerIsIconNode(const RoboXMLNode& node);
bool RoboCompilerIconsLoaded();
bool RoboCompilerIconInCatalog(const char* name);
std::string RoboCompilerCatalogSvg(const char* name);
void RoboCompilerBindSvgName(const RoboXMLNode* node, const std::string& name);
std::string RoboCompilerLookupSvgName(const RoboXMLNode* node);
int RoboCompilerTakeInlineSerial();
bool RoboCompilerInlineSvgTaken(const std::string& name);
void RoboCompilerPutInlineSvg(const std::string& name, const std::string& svg);
std::string RoboCompilerSvgIconName(const RoboXMLNode& node);
void RoboCompilerSerializeSvgNode(const RoboXMLNode& node, std::string& out, bool root);
void RoboCompilerEnsureIcons();
