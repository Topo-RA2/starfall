# Rebuild the optional PCX edition without changing the PNG edition.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationCore,WindowsBase,System.Drawing
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$source=Join-Path $root 'examples/HexBrawlIcons'
$destination=Join-Path $root 'examples/HexBrawlIconsPCX'
$iniPath=Join-Path $root 'examples/Starfall.HexBrawl.ini'
$ini=[IO.File]::ReadAllText($iniPath)
$pattern='(?m)^Icon=HexBrawlIcons\\([A-Za-z0-9_-]+)\.png(?=\r?$)'
$references=[regex]::Matches($ini,$pattern)
$files=@(Get-ChildItem -LiteralPath $source -Filter '*.png' | Sort-Object Name)
if (!$files.Count -or $references.Count -ne $files.Count) {throw 'The INI and icon set must have the same number of entries.'}
foreach ($reference in $references) {
    if (!(Test-Path -LiteralPath (Join-Path $source ($reference.Groups[1].Value+'.png')))) {throw "Missing icon: $reference"}
}
$null=New-Item -ItemType Directory -Force -Path $destination
foreach ($file in $files) {
    $original=[Drawing.Bitmap]::new($file.FullName)
    $rgb=[Drawing.Bitmap]::new($original.Width,$original.Height,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $stream=[IO.MemoryStream]::new()
    try {
        $width=$original.Width;$height=$original.Height
        $visible=[bool[]]::new($width*$height)
        for ($y=0;$y -lt $height;$y++) {for ($x=0;$x -lt $width;$x++) {
            $c=$original.GetPixel($x,$y);$visible[$y*$width+$x]=$c.A -ge 128
            # Quantize straight RGB, not RGB composited against magenta (which produces fringes).
            $rgb.SetPixel($x,$y,$(if ($c.A -ge 128) {[Drawing.Color]::FromArgb($c.R,$c.G,$c.B)} else {[Drawing.Color]::Black}))
        }}
        $rgb.Save($stream,[Drawing.Imaging.ImageFormat]::Png);$stream.Position=0
        $frame=[Windows.Media.Imaging.PngBitmapDecoder]::new($stream,0,2).Frames[0]
        $quantized=[Windows.Media.Imaging.BitmapPalette]::new($frame,255)
        $colors=[Collections.Generic.List[Windows.Media.Color]]::new()
        $colors.Add([Windows.Media.Colors]::Magenta)
        foreach ($c in $quantized.Colors) {
            # Reserve the transparent color even after the engine converts to RGB565/RGB555.
            if ($c.R -ge 248 -and $c.G -lt 8 -and $c.B -ge 248) {$c=[Windows.Media.Color]::FromRgb($c.R,8,$c.B)}
            $colors.Add($c)
        }
        $palette=[Windows.Media.Imaging.BitmapPalette]::new($colors)
        $indexed=[Windows.Media.Imaging.FormatConvertedBitmap]::new($frame,[Windows.Media.PixelFormats]::Indexed8,$palette,0)
        $stride=$width+($width%2);$pixels=[byte[]]::new($height*$stride)
        $indexed.CopyPixels($pixels,$stride,0)
        for ($y=0;$y -lt $height;$y++) {for ($x=0;$x -lt $width;$x++) {
            $i=$y*$stride+$x
            if (!$visible[$y*$width+$x]) {$pixels[$i]=0}
            elseif ($pixels[$i] -eq 0) {
                $c=$original.GetPixel($x,$y);$best=1;$distance=[int]::MaxValue
                for ($p=1;$p -lt $colors.Count;$p++) {
                    $pc=$colors[$p];$d=($c.R-$pc.R)*($c.R-$pc.R)+($c.G-$pc.G)*($c.G-$pc.G)+($c.B-$pc.B)*($c.B-$pc.B)
                    if ($d -lt $distance) {$distance=$d;$best=$p}
                }
                $pixels[$i]=[byte]$best
            }
        }}
        $header=[byte[]]::new(128);$header[0]=10;$header[1]=5;$header[2]=1;$header[3]=8;$header[65]=1
        foreach ($pair in @(@(8,($width-1)),@(10,($height-1)),@(12,72),@(14,72),@(66,$stride),@(68,1))) {
            [BitConverter]::GetBytes([uint16]$pair[1]).CopyTo($header,$pair[0])
        }
        $writer=[IO.BinaryWriter]::new([IO.File]::Create((Join-Path $destination ('hx_'+$file.BaseName.ToLowerInvariant()+'.pcx'))))
        try {
            $writer.Write($header)
            for ($y=0;$y -lt $height;$y++) {for ($x=0;$x -lt $stride;) {
                $value=$pixels[$y*$stride+$x];$length=1
                while ($length -lt 63 -and $x+$length -lt $stride -and $pixels[$y*$stride+$x+$length] -eq $value) {$length++}
                if ($length -gt 1 -or $value -ge 192) {$writer.Write([byte](192+$length))}
                $writer.Write($value);$x+=$length
            }}
            $writer.Write([byte]12);$paletteBytes=[byte[]]::new(768)
            for ($i=0;$i -lt $colors.Count;$i++) {
                $paletteBytes[$i*3]=$colors[$i].R;$paletteBytes[$i*3+1]=$colors[$i].G;$paletteBytes[$i*3+2]=$colors[$i].B
            }
            $writer.Write($paletteBytes)
        } finally {$writer.Dispose()}
    } finally {$original.Dispose();$rgb.Dispose();$stream.Dispose()}
}
$pcxIni=[regex]::Replace($ini,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)
    'Icon=hx_'+$m.Groups[1].Value.ToLowerInvariant()+'.pcx'
})
$pcxIni=[regex]::Replace($pcxIni,'(?m)^;[^\r\n]*HexBrawlIcons[^\r\n]*(?=\r?$)',
    '; Copy the PCX files from HexBrawlIconsPCX directly into the game root, without the folder.')
$pcxIni="; PCX edition: 8-bit palette, magenta transparency, alpha cutoff 128/255.`r`n"+$pcxIni
[IO.File]::WriteAllText((Join-Path $root 'examples/Starfall.HexBrawl.PCX.ini'),$pcxIni,[Text.UTF8Encoding]::new($false))
Write-Host "Converted $($files.Count) icons; wrote examples/Starfall.HexBrawl.PCX.ini."
