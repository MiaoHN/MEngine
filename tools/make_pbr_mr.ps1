# One-off tool: pack LO's separate metallic.png / roughness.png grayscale maps
# into the engine's combined metallic-roughness texture (R=1, G=roughness,
# B=metallic), saved next to each material as mr.png.
Add-Type -AssemblyName System.Drawing

$root = Join-Path $PSScriptRoot '..\assets\textures\pbr'
$root = [System.IO.Path]::GetFullPath($root)

Get-ChildItem $root -Directory | ForEach-Object {
  $rough = Join-Path $_.FullName 'roughness.png'
  $metal = Join-Path $_.FullName 'metallic.png'
  if (-not (Test-Path $rough) -or -not (Test-Path $metal)) { return }
  $rb = New-Object System.Drawing.Bitmap($rough)
  $mb = New-Object System.Drawing.Bitmap($metal)
  $w = $rb.Width; $h = $rb.Height
  if ($mb.Width -ne $w -or $mb.Height -ne $h) {
    Write-Output ("SKIP (size mismatch) " + $_.Name)
    $rb.Dispose(); $mb.Dispose(); return
  }
  $out = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
      $rp = $rb.GetPixel($x, $y)
      $mp = $mb.GetPixel($x, $y)
      # R = 1, G = roughness, B = metallic (engine: roughness *= G, metallic *= B)
      $col = [System.Drawing.Color]::FromArgb(255, 255, $rp.R, $mp.R)
      $out.SetPixel($x, $y, $col)
    }
  }
  $dest = Join-Path $_.FullName 'mr.png'
  $out.Save($dest, [System.Drawing.Imaging.ImageFormat]::Png)
  $out.Dispose(); $rb.Dispose(); $mb.Dispose()
  Write-Output ("made " + $dest)
}
Write-Output "done"
