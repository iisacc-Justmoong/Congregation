#Requires -Version 5.1
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$MsiPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-MsiRows {
    param(
        [Parameter(Mandatory = $true)]
        $Database,

        [Parameter(Mandatory = $true)]
        [string]$Query,

        [Parameter(Mandatory = $true)]
        [string[]]$Columns
    )

    $view = $Database.OpenView($Query)
    try {
        $null = $view.Execute()
        while ($record = $view.Fetch()) {
            $values = [ordered]@{}
            for ($index = 0; $index -lt $Columns.Count; ++$index) {
                $values[$Columns[$index]] = $record.StringData($index + 1)
            }
            [pscustomobject]$values
        }
    } finally {
        $null = $view.Close()
    }
}

$null = Resolve-Path -LiteralPath $BuildDirectory
if (-not (Test-Path -LiteralPath $MsiPath -PathType Leaf)) {
    throw "Requested Congregation MSI does not exist: $MsiPath"
}
$msiFile = Get-Item -LiteralPath $MsiPath

Write-Host "Inspecting MSI database: $($msiFile.FullName)"
$installer = New-Object -ComObject WindowsInstaller.Installer
$database = $installer.OpenDatabase($msiFile.FullName, 0)

$propertyRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Property`,`Value` FROM `Property`' `
        -Columns @("Property", "Value"))
$properties = @{}
foreach ($row in $propertyRows) {
    $properties[$row.Property] = $row.Value
}

Assert-Condition (-not $properties.ContainsKey("ARPNOMODIFY")) `
    "WixUI_InstallDir injected ARPNOMODIFY, so the maintenance Change option is disabled."
Assert-Condition ($properties.ContainsKey("WixUI_Mode") -and $properties["WixUI_Mode"] -eq "Advanced") `
    "The generated MSI must use WixUI_Advanced."
Assert-Condition ($properties.ContainsKey("ProductVersion") -and $properties["ProductVersion"] -eq $Version) `
    "The MSI ProductVersion must match the release version."
Assert-Condition ($properties.ContainsKey("ALLUSERS") -and $properties["ALLUSERS"] -eq "2") `
    "A dual-context package must author ALLUSERS=2."
Assert-Condition ($properties.ContainsKey("MSIINSTALLPERUSER") -and $properties["MSIINSTALLPERUSER"] -eq "1") `
    "A dual-context package must author MSIINSTALLPERUSER=1 so current-user installation is the default."
Assert-Condition ($properties.ContainsKey("WixAppFolder") -and $properties["WixAppFolder"] -eq "WixPerUserFolder") `
    "WixUI_Advanced must default its scope selection to the current user."
Assert-Condition ($properties.ContainsKey("ApplicationFolderName") -and $properties["ApplicationFolderName"] -eq "Congregation") `
    "WixUI_Advanced must define the application folder name."

$appSearchRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Property`,`Signature_` FROM `AppSearch`' `
        -Columns @("Property", "Signature"))
$registryLocatorRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Signature_`,`Root`,`Key`,`Name` FROM `RegLocator`' `
        -Columns @("Signature", "Root", "Key", "Name"))
foreach ($contextSearch in @(
        @{ Property = "CONGREGATION_EXISTING_USER_CONTEXT"; Root = 1; Name = "installContext" },
        @{ Property = "CONGREGATION_LEGACY_USER_CONTEXT"; Root = 1; Name = "installed" },
        @{ Property = "CONGREGATION_EXISTING_USER_INSTALLLOCATION"; Root = 1; Name = "InstallLocation" },
        @{ Property = "CONGREGATION_EXISTING_MACHINE_CONTEXT"; Root = 2; Name = "installContext" },
        @{ Property = "CONGREGATION_LEGACY_MACHINE_CONTEXT"; Root = 2; Name = "machineStartMenuShortcut" },
        @{ Property = "CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION"; Root = 2; Name = "InstallLocation" }
    )) {
    $search = @($appSearchRows | Where-Object { $_.Property -eq $contextSearch.Property })
    Assert-Condition ($search.Count -eq 1) "$($contextSearch.Property) must have exactly one AppSearch row."
    $locator = @($registryLocatorRows | Where-Object { $_.Signature -eq $search[0].Signature })
    Assert-Condition ($locator.Count -eq 1 `
            -and [int]$locator[0].Root -eq $contextSearch.Root `
            -and $locator[0].Key -eq "Software\IISACC\Congregation" `
            -and $locator[0].Name -eq $contextSearch.Name) `
        "$($contextSearch.Property) must search the expected 64-bit installation-context marker."
}
$machineProgramFilesSearch = @($appSearchRows | Where-Object { $_.Property -eq "CONGREGATION_MACHINE_PROGRAMFILES64" })
Assert-Condition ($machineProgramFilesSearch.Count -eq 1) `
    "The native 64-bit machine Program Files path must have exactly one AppSearch row."
$machineProgramFilesLocator = @($registryLocatorRows | Where-Object { $_.Signature -eq $machineProgramFilesSearch[0].Signature })
Assert-Condition ($machineProgramFilesLocator.Count -eq 1 `
        -and [int]$machineProgramFilesLocator[0].Root -eq 2 `
        -and $machineProgramFilesLocator[0].Key -eq "SOFTWARE\Microsoft\Windows\CurrentVersion" `
        -and $machineProgramFilesLocator[0].Name -eq "ProgramFilesDir") `
    "The all-users path must come from the native 64-bit machine ProgramFilesDir."

$scopeFolderActions = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Action`,`Type`,`Source`,`Target` FROM `CustomAction`' `
        -Columns @("Action", "Type", "Source", "Target"))
$nativeMachineFolderActions = @($scopeFolderActions | Where-Object { $_.Action -eq "SetWixPerMachineFolder" })
Assert-Condition ($nativeMachineFolderActions.Count -eq 1 `
        -and [int]$nativeMachineFolderActions[0].Type -eq 51 `
        -and $nativeMachineFolderActions[0].Source -eq "WixPerMachineFolder" `
        -and $nativeMachineFolderActions[0].Target -eq '[CONGREGATION_MACHINE_PROGRAMFILES64]\[ApplicationFolderName]') `
    "All-users scope must override WixUI_Advanced's 32-bit default with native 64-bit Program Files."

foreach ($sequenceTable in @("InstallUISequence", "InstallExecuteSequence")) {
    $scopeFolderSequence = @(Get-MsiRows `
            -Database $database `
            -Query "SELECT ``Action``,``Sequence`` FROM ``$sequenceTable``" `
            -Columns @("Action", "Sequence"))
    $defaultMachineFolderAction = @($scopeFolderSequence | Where-Object { $_.Action -eq "WixSetDefaultPerMachineFolder" })
    $nativeMachineFolderAction = @($scopeFolderSequence | Where-Object { $_.Action -eq "SetWixPerMachineFolder" })
    Assert-Condition ($defaultMachineFolderAction.Count -eq 1 `
            -and $nativeMachineFolderAction.Count -eq 1 `
            -and [int]$nativeMachineFolderAction[0].Sequence -gt [int]$defaultMachineFolderAction[0].Sequence) `
        "Native 64-bit folder selection must override WixUI_Advanced in $sequenceTable."
}

$installDirectories = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Directory``,``Directory_Parent`` FROM ``Directory`` WHERE ``Directory``='APPLICATIONFOLDER'" `
        -Columns @("Directory", "Parent"))
Assert-Condition ($installDirectories.Count -eq 1 -and $installDirectories[0].Parent -eq "ProgramFiles64Folder") `
    "APPLICATIONFOLDER must use the context-aware 64-bit Program Files directory."

$shortcutDirectories = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Directory``,``Directory_Parent`` FROM ``Directory``" `
        -Columns @("Directory", "Parent"))
$applicationShortcutDirectories = @($shortcutDirectories | Where-Object { $_.Directory -eq "ApplicationProgramsFolder" })
$machineShortcutDirectories = @($shortcutDirectories | Where-Object { $_.Directory -eq "MachineApplicationProgramsFolder" })
Assert-Condition ($applicationShortcutDirectories.Count -eq 1 -and $applicationShortcutDirectories[0].Parent -eq "ProgramMenuFolder") `
    "The Start Menu shortcut must use the context-aware ProgramMenuFolder."
Assert-Condition ($machineShortcutDirectories.Count -eq 0) `
    "The package must not synthesize a nonstandard machine-only Programs directory."

$registryRows = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Registry``,``Root``,``Key``,``Name``,``Component_`` FROM ``Registry``" `
        -Columns @("Registry", "Root", "Key", "Name", "Component"))
Assert-Condition (@($registryRows | Where-Object {
            $_.Component -in @("ApplicationShortcutComponent", "MachineStartMenuShortcutComponent")
        }).Count -eq 0) `
    "The dual-context package must not reuse fixed-scope non-advertised shortcut components."

$shortcutRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Shortcut`,`Directory_`,`Component_`,`Name`,`Target` FROM `Shortcut`' `
        -Columns @("Shortcut", "Directory", "Component", "Name", "Target"))
$applicationShortcutRows = @($shortcutRows | Where-Object { $_.Shortcut -eq "ApplicationStartMenuShortcut" })
Assert-Condition ($applicationShortcutRows.Count -eq 1 `
        -and $applicationShortcutRows[0].Directory -eq "ApplicationProgramsFolder" `
        -and $applicationShortcutRows[0].Target -eq "CoreFeature") `
    "The context-aware Start Menu entry must be authored as an advertised CoreFeature shortcut."
Assert-Condition ($properties.ContainsKey("DISABLEADVTSHORTCUTS") -and $properties["DISABLEADVTSHORTCUTS"] -eq "1") `
    "Windows Installer must materialize the validated advertised entry as a normal shell shortcut."

$componentConditions = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Component`,`ComponentId`,`Directory_`,`Condition`,`KeyPath` FROM `Component`' `
        -Columns @("Component", "ComponentId", "Directory", "Condition", "KeyPath"))
$advertisedShortcutComponents = @($componentConditions | Where-Object {
        $_.Component -eq $applicationShortcutRows[0].Component
    })
$installContextComponents = @($componentConditions | Where-Object { $_.Component -eq "InstallContextComponent" })
Assert-Condition ($advertisedShortcutComponents.Count -eq 1 `
        -and $advertisedShortcutComponents[0].Directory -eq "APPLICATIONFOLDER" `
        -and [string]::IsNullOrEmpty($advertisedShortcutComponents[0].Condition) `
        -and -not [string]::IsNullOrEmpty($advertisedShortcutComponents[0].KeyPath)) `
    "The Start Menu shortcut must follow the Congregation executable component in both contexts."
$shortcutTargetFiles = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `File`,`Component_`,`FileName` FROM `File`' `
        -Columns @("File", "Component", "Name") | Where-Object {
            $_.File -eq $advertisedShortcutComponents[0].KeyPath
        })
Assert-Condition ($shortcutTargetFiles.Count -eq 1 `
        -and $shortcutTargetFiles[0].Component -eq $applicationShortcutRows[0].Component `
        -and $shortcutTargetFiles[0].Name -match '(^|\|)Congregation\.exe$') `
    "The advertised shortcut component key path must be Congregation.exe."
$packagedFiles = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `FileName` FROM `File`' `
        -Columns @("Name"))
foreach ($requiredLegalFileName in @(
        "LICENSE.txt",
        "THIRD_PARTY_NOTICES.txt",
        "SOURCE_OFFER.txt",
        "COPYING.txt",
        "miniz-Unlicense.txt",
        "GCC-Runtime-Library-Exception-3.1.txt",
        "COPYING.MinGW-w64-runtime.txt",
        "winpthreads-COPYING.txt"
    )) {
    Assert-Condition `
        (@($packagedFiles | Where-Object { $_.Name -match "(^|\|)$([regex]::Escape($requiredLegalFileName))$" }).Count -ge 1) `
        "The MSI must install required legal material: $requiredLegalFileName"
}
Assert-Condition ($installContextComponents.Count -eq 1 `
        -and $installContextComponents[0].ComponentId -eq "{05DF7FB6-91BE-5013-AE84-E4A6496550E8}" `
        -and $installContextComponents[0].Directory -eq "APPLICATIONFOLDER") `
    "The required installation-context marker must have a stable component identity."
$installContextRegistryRows = @($registryRows | Where-Object {
        $_.Component -eq "InstallContextComponent" -and $_.Name -eq "installContext"
    })
Assert-Condition ($installContextRegistryRows.Count -eq 1 `
        -and [int]$installContextRegistryRows[0].Root -eq -1 `
        -and $installContextRegistryRows[0].Name -eq "installContext" `
        -and $installContextComponents[0].KeyPath -eq $installContextRegistryRows[0].Registry) `
    "The installation-context marker must use an ICE105-compatible HKMU key path."

$scopeDialogs = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Dialog`` FROM ``Dialog`` WHERE ``Dialog``='InstallScopeDlg'" `
        -Columns @("Dialog"))
Assert-Condition ($scopeDialogs.Count -eq 1) `
    "WixUI_Advanced must provide InstallScopeDlg."

$selectionTrees = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Dialog_``,``Control``,``Type`` FROM ``Control`` WHERE ``Dialog_``='FeaturesDlg' AND ``Type``='SelectionTree'" `
        -Columns @("Dialog", "Control", "Type"))
Assert-Condition ($selectionTrees.Count -eq 1) `
    "FeaturesDlg must contain exactly one SelectionTree control."

$scopeControls = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Control``,``Type`` FROM ``Control`` WHERE ``Dialog_``='InstallScopeDlg'" `
        -Columns @("Control", "Type"))
Assert-Condition (@($scopeControls | Where-Object { $_.Control -eq "BothScopes" -and $_.Type -eq "RadioButtonGroup" }).Count -eq 1) `
    "InstallScopeDlg must offer current-user and all-users scope choices."

$scopeLockEvents = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Event``,``Argument``,``Condition``,``Ordering`` FROM ``ControlEvent`` WHERE ``Dialog_``='InstallScopeDlg' AND ``Control_``='Next'" `
        -Columns @("Event", "Argument", "Condition", "Ordering"))
Assert-Condition (@($scopeLockEvents | Where-Object {
                $_.Event -eq "[WixAppFolder]" `
                -and $_.Argument -eq "WixPerUserFolder" `
                -and $_.Condition.Contains("CONGREGATION_EXISTING_USER_CONTEXT") `
                -and $_.Condition.Contains("WIX_UPGRADE_DETECTED") `
                -and $_.Condition.Contains("CONGREGATION_EXISTING_MACHINE_CONTEXT") `
                -and [int]$_.Ordering -lt 1
            }).Count -eq 1) `
    "An existing or markerless detected current-user installation must lock the scope before WixUI_Advanced processes it."
Assert-Condition (@($scopeLockEvents | Where-Object {
                $_.Event -eq "[WixAppFolder]" `
                -and $_.Argument -eq "WixPerMachineFolder" `
                -and $_.Condition.Contains("CONGREGATION_EXISTING_MACHINE_CONTEXT") `
                -and [int]$_.Ordering -lt 1
            }).Count -eq 1) `
    "An existing all-users installation must lock the scope before WixUI_Advanced processes it."
Assert-Condition (@($scopeLockEvents | Where-Object {
                $_.Event -eq "[APPLICATIONFOLDER]" `
                -and $_.Argument -eq "[CONGREGATION_EXISTING_USER_INSTALLLOCATION]" `
                -and $_.Condition.Contains("CONGREGATION_EXISTING_USER_INSTALLLOCATION") `
                -and [int]$_.Ordering -eq 5
            }).Count -eq 1) `
    "A current-user upgrade must preserve its registered installation directory."
Assert-Condition (@($scopeLockEvents | Where-Object {
                $_.Event -eq "[APPLICATIONFOLDER]" `
                -and $_.Argument -eq "[CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION]" `
                -and $_.Condition.Contains("CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION") `
                -and [int]$_.Ordering -eq 6
            }).Count -eq 1) `
    "An all-users upgrade must preserve its registered installation directory."

$scopeCommitEvents = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Control_``,``Event``,``Argument``,``Condition``,``Ordering`` FROM ``ControlEvent`` WHERE ``Dialog_``='FeaturesDlg'" `
        -Columns @("Control", "Event", "Argument", "Condition", "Ordering"))
foreach ($installControl in @("Install", "InstallNoShield")) {
    $controlEvents = @($scopeCommitEvents | Where-Object { $_.Control -eq $installControl })
    Assert-Condition (@($controlEvents | Where-Object {
                    $_.Event -eq "[MSIINSTALLPERUSER]" `
                    -and $_.Argument -eq "1" `
                    -and $_.Condition -eq 'NOT Installed AND WixAppFolder = "WixPerUserFolder"' `
                    -and [int]$_.Ordering -lt 2
                }).Count -eq 1) `
        "$installControl must commit the official per-user MSIINSTALLPERUSER value before installation starts."
    Assert-Condition (@($controlEvents | Where-Object {
                    $_.Event -eq "[MSIINSTALLPERUSER]" `
                    -and $_.Argument -eq "{}" `
                    -and $_.Condition -eq 'NOT Installed AND WixAppFolder = "WixPerMachineFolder"' `
                    -and [int]$_.Ordering -lt 2
                }).Count -eq 1) `
        "$installControl must clear MSIINSTALLPERUSER for an all-users install."
    Assert-Condition (@($controlEvents | Where-Object {
                    $_.Event -eq "[ALLUSERS]" `
                    -and $_.Argument -eq "2" `
                    -and $_.Condition -eq "NOT Installed" `
                    -and [int]$_.Ordering -lt 2
            }).Count -eq 1) `
        "$installControl must restore ALLUSERS=2 before Windows Installer selects the context."
    Assert-Condition (@($controlEvents | Where-Object {
                    $_.Event -eq "[APPLICATIONFOLDER]" `
                    -and $_.Argument -eq "[CONGREGATION_EXISTING_USER_INSTALLLOCATION]" `
                    -and $_.Condition.Contains("CONGREGATION_EXISTING_USER_INSTALLLOCATION") `
                    -and [int]$_.Ordering -lt 2
                }).Count -eq 1) `
        "$installControl must preserve an existing current-user installation directory."
    Assert-Condition (@($controlEvents | Where-Object {
                    $_.Event -eq "[APPLICATIONFOLDER]" `
                    -and $_.Argument -eq "[CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION]" `
                    -and $_.Condition.Contains("CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION") `
                    -and [int]$_.Ordering -lt 2
                }).Count -eq 1) `
        "$installControl must preserve an existing all-users installation directory."
}

$installDirectoryControls = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Control``,``Type`` FROM ``Control`` WHERE ``Dialog_``='InstallDirDlg'" `
        -Columns @("Control", "Type"))
Assert-Condition (@($installDirectoryControls | Where-Object { $_.Control -eq "ChangeFolder" -and $_.Type -eq "PushButton" }).Count -eq 1) `
    "InstallDirDlg must expose a destination-folder chooser for all-users installation."

$maintenanceChangeEvents = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Event``,``Argument`` FROM ``ControlEvent`` WHERE ``Dialog_``='MaintenanceTypeDlg' AND ``Control_``='ChangeButton'" `
        -Columns @("Event", "Argument"))
Assert-Condition (@($maintenanceChangeEvents | Where-Object { $_.Event -eq "NewDialog" -and $_.Argument -eq "FeaturesDlg" }).Count -eq 1) `
    "The maintenance Change option must open FeaturesDlg."

$licenseControls = @(Get-MsiRows `
        -Database $database `
        -Query "SELECT ``Text`` FROM ``Control`` WHERE ``Dialog_``='AdvancedWelcomeEulaDlg' AND ``Control``='LicenseText'" `
        -Columns @("Text"))
Assert-Condition ($licenseControls.Count -eq 1) `
    "AdvancedWelcomeEulaDlg must contain exactly one LicenseText control."
Assert-Condition ($licenseControls[0].Text.Contains("GNU AFFERO GENERAL PUBLIC LICENSE")) `
    "The MSI must embed the repository's GNU AGPL license text."
Assert-Condition (-not $licenseControls[0].Text.Contains("Lorem ipsum")) `
    "The MSI must not ship WiX's placeholder Lorem Ipsum license text."

$featureRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Feature`,`Feature_Parent`,`Level`,`Directory_`,`Attributes` FROM `Feature`' `
        -Columns @("Feature", "Parent", "Level", "Directory", "Attributes"))
$coreFeatures = @($featureRows | Where-Object { $_.Feature -eq "CoreFeature" })

Assert-Condition ($coreFeatures.Count -eq 1) "The MSI must contain exactly one CoreFeature."
Assert-Condition ([int]$coreFeatures[0].Level -eq 1) "CoreFeature must be selected by default."
Assert-Condition (([int]$coreFeatures[0].Attributes -band 16) -eq 16) `
    "CoreFeature must disallow an absent state."
Assert-Condition ($coreFeatures[0].Directory -eq "APPLICATIONFOLDER") `
    "CoreFeature must make APPLICATIONFOLDER configurable on the first install."

Assert-Condition (@($featureRows | Where-Object { $_.Feature -eq "StartMenuShortcutFeature" }).Count -eq 0) `
    "The shortcut must not be split into a fixed-scope feature that violates dual-context component rules."

$featureComponentRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Feature_`,`Component_` FROM `FeatureComponents`' `
        -Columns @("Feature", "Component"))
$advertisedShortcutComponentOwners = @($featureComponentRows | Where-Object {
        $_.Component -eq $applicationShortcutRows[0].Component
    })
Assert-Condition ($advertisedShortcutComponentOwners.Count -eq 1 `
        -and $advertisedShortcutComponentOwners[0].Feature -eq "CoreFeature") `
    "The executable-backed advertised shortcut must belong to required CoreFeature."

$coreComponents = @($featureComponentRows | Where-Object { $_.Feature -eq "CoreFeature" })
Assert-Condition ($coreComponents.Count -gt 0) "CoreFeature must own the application runtime components."
Assert-Condition (@($coreComponents | Where-Object { $_.Component -eq "InstallContextComponent" }).Count -eq 1) `
    "CoreFeature must always install the context marker."

$upgradeRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `VersionMin`,`VersionMax`,`Attributes`,`ActionProperty` FROM `Upgrade`' `
        -Columns @("VersionMin", "VersionMax", "Attributes", "ActionProperty"))
$upgradeDetectionRows = @($upgradeRows | Where-Object { $_.ActionProperty -eq "WIX_UPGRADE_DETECTED" })
Assert-Condition ($upgradeDetectionRows.Count -eq 1) `
    "The MSI must contain exactly one major-upgrade detection row."
Assert-Condition ($upgradeDetectionRows[0].VersionMax -eq $Version) `
    "The major-upgrade range must end at the current release version."
Assert-Condition (([int]$upgradeDetectionRows[0].Attributes -band 512) -eq 0) `
    "The release MSI must not replace another package with the same ProductVersion."

$executeSequenceRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Action`,`Sequence` FROM `InstallExecuteSequence`' `
        -Columns @("Action", "Sequence"))
$removeExistingProducts = @($executeSequenceRows | Where-Object { $_.Action -eq "RemoveExistingProducts" })
$installInitialize = @($executeSequenceRows | Where-Object { $_.Action -eq "InstallInitialize" })
$installFinalize = @($executeSequenceRows | Where-Object { $_.Action -eq "InstallFinalize" })
$executeAppSearch = @($executeSequenceRows | Where-Object { $_.Action -eq "AppSearch" })
$executeScopeLock = @($executeSequenceRows | Where-Object { $_.Action -eq "CongregationSetExistingMachineContext" })
$executeFindRelatedProducts = @($executeSequenceRows | Where-Object { $_.Action -eq "FindRelatedProducts" })
Assert-Condition ($removeExistingProducts.Count -eq 1) `
    "The MSI must schedule exactly one RemoveExistingProducts action."
Assert-Condition ($installInitialize.Count -eq 1 -and $installFinalize.Count -eq 1) `
    "The MSI transaction boundaries are missing."
Assert-Condition ($executeAppSearch.Count -eq 1 -and [int]$executeAppSearch[0].Sequence -eq 10) `
    "Execute AppSearch must detect context markers before FindRelatedProducts."
Assert-Condition ($executeScopeLock.Count -eq 1 `
        -and [int]$executeScopeLock[0].Sequence -gt [int]$executeAppSearch[0].Sequence `
        -and $executeFindRelatedProducts.Count -eq 1 `
        -and [int]$executeScopeLock[0].Sequence -lt [int]$executeFindRelatedProducts[0].Sequence) `
    "The existing machine context must be restored before upgrade detection."
Assert-Condition ([int]$removeExistingProducts[0].Sequence -gt [int]$installInitialize[0].Sequence) `
    "RemoveExistingProducts must run after InstallInitialize so a failed upgrade can roll back the old product."
Assert-Condition ([int]$removeExistingProducts[0].Sequence -lt [int]$installFinalize[0].Sequence) `
    "RemoveExistingProducts must remain inside the Windows Installer transaction."

$uiSequenceRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Action`,`Condition`,`Sequence` FROM `InstallUISequence`' `
        -Columns @("Action", "Condition", "Sequence"))
$uiFindRelatedProducts = @($uiSequenceRows | Where-Object { $_.Action -eq "FindRelatedProducts" })
Assert-Condition ($uiFindRelatedProducts.Count -eq 1 `
        -and [int]$uiFindRelatedProducts[0].Sequence -eq 25 `
        -and [string]::IsNullOrEmpty($uiFindRelatedProducts[0].Condition)) `
    "UI FindRelatedProducts must run only after the existing installation scope has been locked."
$uiAppSearch = @($uiSequenceRows | Where-Object { $_.Action -eq "AppSearch" })
Assert-Condition ($uiAppSearch.Count -eq 1 -and [int]$uiAppSearch[0].Sequence -eq 10) `
    "UI AppSearch must detect and lock an existing installation scope before showing dialogs."
$uiScopeLockActions = @($uiSequenceRows | Where-Object {
        $_.Action -in @("CongregationSetExistingUserUiScope", "CongregationSetExistingMachineUiScope", "CongregationSetExistingMachineContext")
    })
Assert-Condition ($uiScopeLockActions.Count -eq 3 `
        -and @($uiScopeLockActions | Where-Object {
                [int]$_.Sequence -le [int]$uiAppSearch[0].Sequence `
                -or [int]$_.Sequence -ge [int]$uiFindRelatedProducts[0].Sequence
            }).Count -eq 0) `
    "Every UI scope lock must run after AppSearch and before FindRelatedProducts."
$detectedUserScopeLocks = @($uiSequenceRows | Where-Object { $_.Action -eq "CongregationSetDetectedUserUiScope" })
Assert-Condition ($detectedUserScopeLocks.Count -eq 1 `
        -and [int]$detectedUserScopeLocks[0].Sequence -gt [int]$uiFindRelatedProducts[0].Sequence `
        -and $detectedUserScopeLocks[0].Condition.Contains("WIX_UPGRADE_DETECTED") `
        -and $detectedUserScopeLocks[0].Condition.Contains("CONGREGATION_EXISTING_MACHINE_CONTEXT")) `
    "A markerless related product must lock the UI to current-user scope after upgrade detection."

$launchConditionRows = @(Get-MsiRows `
        -Database $database `
        -Query 'SELECT `Condition`,`Description` FROM `LaunchCondition`' `
        -Columns @("Condition", "Description"))
$ambiguousContextGuards = @($launchConditionRows | Where-Object {
                $_.Condition.Contains("CONGREGATION_EXISTING_USER_CONTEXT") `
                -and $_.Condition.Contains("CONGREGATION_EXISTING_MACHINE_CONTEXT")
            })
Assert-Condition ($ambiguousContextGuards.Count -ge 1) `
    "The MSI must reject ambiguous side-by-side user and machine registrations."
Assert-Condition (@($ambiguousContextGuards | Where-Object {
                $_.Condition.Contains("Installed") `
                -and $_.Condition.Contains('REMOVE~="ALL"')
            }).Count -eq $ambiguousContextGuards.Count) `
    "Cross-context launch guards must allow maintenance and removal so an ambiguous registration can be recovered."

foreach ($conditionTable in @("ControlEvent", "InstallUISequence", "InstallExecuteSequence", "LaunchCondition")) {
    $conditionRows = @(Get-MsiRows `
            -Database $database `
            -Query "SELECT ``Condition`` FROM ``$conditionTable``" `
            -Columns @("Condition"))
    Assert-Condition (@($conditionRows | Where-Object { $_.Condition.Length -gt 255 }).Count -eq 0) `
        "$conditionTable contains a condition that exceeds the 255-character MSI column limit."
}

Write-Host "MSI install-options database contract passed."
