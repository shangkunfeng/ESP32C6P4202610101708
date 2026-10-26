$ErrorActionPreference = "Stop"
$base = Split-Path -Parent $PSScriptRoot
$symFile = Join-Path $base "tools\home_font_symbols.txt"
$symRaw = (Get-Content -Path $symFile -Encoding UTF8 -Raw).Trim()
$seen = [System.Collections.Generic.HashSet[char]]::new()
$symBuilder = [System.Text.StringBuilder]::new()
foreach ($c in $symRaw.ToCharArray()) {
    if ($seen.Add($c)) {
        [void]$symBuilder.Append($c)
    }
}
$sym = $symBuilder.ToString()
$conv = Join-Path $base "tools\lv_font_conv\package\lv_font_conv.js"
$font = "C:\Windows\Fonts\simhei.ttf"
$out16 = Join-Path $base "main\Platform\ui_lvgl\ui_home\fonts\ui_font_home_16.c"
$out22 = Join-Path $base "main\Platform\ui_lvgl\ui_home\fonts\ui_font_home_22.c"

$includeBlock = @'
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
'@

function Fix-FontIncludes($path) {
    $c = Get-Content -Path $path -Encoding UTF8 -Raw
    $c = [regex]::Replace($c, '#ifdef LV_LVGL_H_INCLUDE_SIMPLE[\s\S]*?#endif\r?\n\r?\n', $includeBlock + "`n`n", 1)
    Set-Content -Path $path -Value $c -Encoding UTF8 -NoNewline
}

node $conv --bpp 4 --size 16 --no-compress --no-prefilter --font $font -r 0x20-0x7F --symbols $sym --format lvgl -o $out16 --force-fast-kern-format
node $conv --bpp 4 --size 22 --no-compress --no-prefilter --font $font -r 0x20-0x7F --symbols $sym --format lvgl -o $out22 --force-fast-kern-format
Fix-FontIncludes $out16
Fix-FontIncludes $out22
Write-Host "Fonts regenerated with UTF-8 symbols"
