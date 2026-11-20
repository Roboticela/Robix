"use strict";

const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");

const extensionRoot = path.resolve(__dirname, "../../Editors/vscode");
const manifest = JSON.parse(fs.readFileSync(path.join(extensionRoot, "package.json"), "utf8"));
const stage = path.resolve(__dirname, "../../dist/vsix");
const output = path.resolve(__dirname, `../../dist/${manifest.publisher}.${manifest.name}.vsix`);

const include = [
    "package.json",
    "extension.js",
    "README.md",
    "language-configuration.json",
    "language-configuration-project.json",
    "lib",
    "syntaxes",
    "themes",
    "icons"
];

fs.rmSync(stage, { recursive: true, force: true });
fs.mkdirSync(path.join(stage, "extension"), { recursive: true });
for (const item of include) {
    const from = path.join(extensionRoot, item);
    if (!fs.existsSync(from)) {
        throw new Error(`Missing ${from}`);
    }
    fs.cpSync(from, path.join(stage, "extension", item), { recursive: true });
}

const description = String(manifest.description || "").replace(/&/g, "&amp;").replace(/</g, "&lt;");
const tags = (manifest.keywords || []).join(",");
const icon = String(manifest.icon || "").replace(/\\/g, "/").replace(/^\.\//, "");
if (!icon || path.extname(icon).toLowerCase() !== ".png") {
    throw new Error("package.json icon must be a PNG path. The marketplace will not load an SVG.");
}
const iconInPackage = path.join(stage, "extension", icon);
if (!fs.existsSync(iconInPackage)) {
    throw new Error(`Marketplace icon was not packaged: ${icon}`);
}
fs.writeFileSync(path.join(stage, "[Content_Types].xml"), `<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="vsixmanifest" ContentType="text/xml"/>
  <Default Extension="json" ContentType="application/json"/>
  <Default Extension="js" ContentType="application/javascript"/>
  <Default Extension="md" ContentType="text/markdown"/>
  <Default Extension="svg" ContentType="image/svg+xml"/>
  <Default Extension="png" ContentType="image/png"/>
  <Default Extension="txt" ContentType="text/plain"/>
  <Default Extension="xml" ContentType="text/xml"/>
</Types>
`);
fs.writeFileSync(path.join(stage, "extension.vsixmanifest"), `<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata>
    <Identity Language="en-US" Id="${manifest.name}" Version="${manifest.version}" Publisher="${manifest.publisher}"/>
    <DisplayName>${manifest.displayName}</DisplayName>
    <Description xml:space="preserve">${description}</Description>
    <Tags>${tags}</Tags>
    <Categories>Programming Languages,Formatters</Categories>
    <Icon>extension/${icon}</Icon>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code"/>
  </Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Icons.Default" Path="extension/${icon}" Addressable="true"/>
  </Assets>
</PackageManifest>
`);

fs.mkdirSync(path.dirname(output), { recursive: true });
fs.rmSync(output, { force: true });
const zip = spawnSync("zip", ["-r", output, ".", "-x", "*.DS_Store"], { cwd: stage, encoding: "utf8" });
if (zip.status !== 0) {
    throw new Error(zip.stderr || zip.stdout || "zip failed");
}
console.log(`Wrote ${output}`);
