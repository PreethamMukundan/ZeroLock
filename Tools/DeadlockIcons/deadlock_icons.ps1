# Downloads Deadlock ability and item icons from deadlock-api.com and imports them into /Game/Data/Images and
# /Game/Data/ShopImages under the names the project already uses, so existing references pick up new art.
# Usage (via deadlock_icons.bat):  deadlock_icons.bat          only icons the project doesn't have yet
#                                  deadlock_icons.bat --all    every icon (refreshes existing art in place)
#                                  deadlock_icons.bat --download-only
param()
$ErrorActionPreference = 'Stop'

$Root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Project = Join-Path $Root 'ZeroLock.uproject'
$Stage = Join-Path $Root 'Saved\DeadlockIcons'
$ApiUrl = 'https://api.deadlock-api.com/v1/assets/items'
$EditorCmd = 'Z:\epicgames\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

$All = $args -contains '--all'
$DownloadOnly = $args -contains '--download-only'

# Folder per icon kind: API field, Content folder, asset name suffix.
$Kinds = @(
    @{ Field = 'image';      Folder = 'Images';     Suffix = '' },
    @{ Field = 'shop_image'; Folder = 'ShopImages'; Suffix = '_shop' }
)

# Unreal's invalid object-name characters become '_' (what the editor did when these were first imported:
# "Chapter's End" -> Chapter_s_End, "Censure / Penance" -> Censure___Penance; '-', '+' and accents stay).
$Invalid = '[\s"''\u2018\u2019,.:|&!~@#(){}\[\]=;^%$`\\/*?<>]'
function AssetName([string]$Name) { return ($Name -replace $Invalid, '_') }

Write-Host "Fetching $ApiUrl ..."
New-Item -ItemType Directory -Force $Stage | Out-Null
$JsonPath = Join-Path $Stage 'items.json'
$Web = New-Object System.Net.WebClient
$Web.Encoding = [System.Text.Encoding]::UTF8
[System.IO.File]::WriteAllText($JsonPath, $Web.DownloadString($ApiUrl), [System.Text.Encoding]::UTF8)
$Items = Get-Content $JsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
Write-Host "$($Items.Count) items/abilities in the API"

$Total = 0
foreach ($Kind in $Kinds) {
    $Out = Join-Path $Stage $Kind.Folder
    if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
    New-Item -ItemType Directory -Force $Out | Out-Null
    $ContentDir = Join-Path $Root "Content\Data\$($Kind.Folder)"

    $Seen = @{}
    $New = 0; $Refreshed = 0; $Failed = 0
    foreach ($Item in $Items) {
        $Url = $Item.($Kind.Field)
        if (-not $Item.name -or -not $Url -or -not $Url.EndsWith('.png')) { continue }
        $Name = (AssetName $Item.name) + $Kind.Suffix
        if ($Seen.ContainsKey($Name)) { continue }   # several entries share a name: the first one with a png wins
        $Seen[$Name] = $true

        $Exists = Test-Path (Join-Path $ContentDir "$Name.uasset")
        if ($Exists -and -not $All) { continue }
        try {
            $Web.DownloadFile($Url, (Join-Path $Out "$Name.png"))
            if ($Exists) { $Refreshed++ } else { $New++; Write-Host "  new: $($Kind.Folder)/$Name" }
        } catch {
            $Failed++
            Write-Warning "  failed: $Name ($Url): $($_.Exception.Message)"
        }
    }
    Write-Host "$($Kind.Folder): $New new, $Refreshed to refresh, $Failed failed"
    $Total += $New + $Refreshed
}

if ($Total -eq 0) {
    Write-Host 'Nothing to import: the project has every icon (use --all to refresh existing art).'
    exit 0
}
$Script = (Join-Path $PSScriptRoot 'ue_import_icons.py') -replace '\\', '/'
if ($DownloadOnly) {
    Write-Host "Downloaded to $Stage. Import in the editor console:  py `"$Script`""
    exit 0
}
if (Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue) {
    Write-Host ''
    Write-Host "The editor is open. Import from its console (Cmd box):"
    Write-Host "  py `"$Script`""
    exit 0
}

Write-Host 'Editor closed: importing headlessly (about a minute to start)...'
& $EditorCmd $Project -run=pythonscript "-script=$Script" -unattended -nosplash -nullrhi 2>&1 |
    Where-Object { $_ -match '\[icons\]' } | ForEach-Object { Write-Host $_ }
exit $LASTEXITCODE
