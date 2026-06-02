Get-ChildItem -Recurse -File | Where-Object {
    $_.FullName -notlike "*node_modules*" -and
    $_.FullName -notlike "*.vs*" -and
    $_.FullName -notlike "*.git*" -and
    $_.Extension -notin @(".lib", ".dll", ".exe", ".obj", ".pdb", ".png", ".jpg", ".zip", ".tar", ".gz")
} | Select-String -Pattern "decode" | Select-Object Path, LineNumber, Line
