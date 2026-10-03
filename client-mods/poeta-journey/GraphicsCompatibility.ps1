function Get-GraphicsCompatibilityPaths($Manifest) {
    if (-not $Manifest.graphicsCompatibility) { return @() }
    $name = $Manifest.graphicsCompatibility.backupName
    if ($name -notmatch '^service-menu-graphics-\d{8}-\d{6}-\d{6}$') { throw 'Unexpected graphics compatibility backup name.' }
    $base = 'DXVK-backups/' + $name
    $paths = @('DXVK/installed.json', 'DXVK/graphics-menu/installed.json',
        'DXVK/graphics-menu/package/manifest.json', 'DXVK/graphics-menu/package/bin64/Game.dll',
        'DXVK/graphics-menu/package/Data/ui/game/game.pak',
        "$base/bin64/Game.dll", "$base/Data/ui/game/game.pak", "$base/cursor-base/bin64/Game.dll")
    if ($Manifest.graphicsCompatibility.localized) {
        $paths += @('DXVK/graphics-menu/package/L10N/enu/Data/data.pak', "$base/L10N/enu/Data/data.pak")
    }
    return $paths
}
