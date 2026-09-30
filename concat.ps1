<#
This script outputs the directory tree and then reads the contents of specific files.
Add new extensions to the array below to expand the search scope.
#>

$extensions = @("*.cpp", "*.h")

Write-Output "=== Directory Structure ==="
cmd.exe /c tree /F /A

Write-Output "`n=== File Contents ==="
Get-ChildItem -Path . -Recurse -Include $extensions | ForEach-Object {
    Write-Output "`n--- File: $($_.FullName) ---"
    Get-Content $_.FullName
}