<#
.SYNOPSIS Launch Developer PowerShell
.DESCRIPTION
Locates and imports a Developer PowerShell module and calls the Enter-VsDevShell cmdlet. The Developer PowerShell module
is located in one of several ways:
  1) From a path in a Visual Studio installation
  2) From the latest installation of Visual Studio (higher versions first)
  3) From the instance ID of a Visual Studio installation
  4) By selecting a Visual Studio installation from a list

By default, with no parameters, the path to this script is used to locate the Developer PowerShell module. If that fails,
then the latest Visual Studio installation is used. If both methods fail, then the user can select a Visual Studio installation
from a list.
.PARAMETER VsInstallationPath 
A path in a Visual Studio installation. The path is used to locate the Developer PowerShell module.
By default, this is the path to this script.
.PARAMETER Latest
Use the latest Visual Studio installation to locate the Developer PowerShell module.
.PARAMETER List
Display a list of Visual Studio installations to choose from. The choosen installation is used to locate the Developer PowerShell module.
.PARAMETER VsInstanceId
A Visual Studio installation instance ID. The matching installation is used to locate the Developer PowerShell module.
.PARAMETER ExcludePrerelease
Excludes Prerelease versions of Visual Studio from consideration. Applies only to Latest and List.
.PARAMETER VsWherePath
Path to the vswhere utility used to located and identify Visual Studio installations.
By default, the path is the well-known location shared by Visual Studio installations.
#>
[CmdletBinding(DefaultParameterSetName = "Default")]
param (
    [ValidateScript({Test-Path $_})]
    [Parameter(ParameterSetName = "VsInstallationPath")]
    [string]
    $VsInstallationPath = "$($MyInvocation.MyCommand.Definition)",

    [Parameter(ParameterSetName = "Latest")]
    [switch]
    $Latest,

    [Parameter(ParameterSetName = "List")]
    [switch]
    $List,

    [Parameter(ParameterSetName = "List")]
    [object[]]
    $DisplayProperties = @("displayName", "instanceId", "installationVersion", "isPrerelease", "installationName", "installDate"),

    [Parameter(ParameterSetName = "VsInstanceId", Mandatory = $true)]
    [string]
    $VsInstanceId,

    [Parameter(ParameterSetName = "Latest")]
    [Parameter(ParameterSetName = "List")]
    [switch]
    $ExcludePrerelease,

    [Parameter(ParameterSetName = "Default")]
    [Parameter(ParameterSetName = "VsInstallationPath")]
    [Parameter(ParameterSetName = "Latest")]
    [Parameter(ParameterSetName = "List")]
    [Parameter(ParameterSetName = "VsInstanceId")]
    [ValidateSet('x86','amd64','arm','arm64')]
    [string]
    $Arch,

    [Parameter(ParameterSetName = "Default")]
    [Parameter(ParameterSetName = "VsInstallationPath")]
    [Parameter(ParameterSetName = "Latest")]
    [Parameter(ParameterSetName = "List")]
    [Parameter(ParameterSetName = "VsInstanceId")]
    [ValidateSet('x86','amd64')]
    [string]
    $HostArch,

    [Parameter(ParameterSetName = "Default")]
    [Parameter(ParameterSetName = "VsInstallationPath")]
    [Parameter(ParameterSetName = "Latest")]
    [Parameter(ParameterSetName = "List")]
    [Parameter(ParameterSetName = "VsInstanceId")]
    [switch]
    $SkipAutomaticLocation,

    [ValidateScript({Test-Path $_ -PathType 'Leaf'})]
    [Parameter(ParameterSetName = "Default")]
    [Parameter(ParameterSetName = "VsInstallationPath")]
    [Parameter(ParameterSetName = "Latest")]
    [Parameter(ParameterSetName = "List")]
    [Parameter(ParameterSetName = "VsInstanceId")]
    [string]
    $VsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
)

function GetSetupConfigurations {
    param (
        $whereArgs
    )
    
    $expression = "& `"$VsWherePath`" $whereArgs -format json"
    Invoke-Expression $expression | ConvertFrom-Json
}

function LaunchDevShell {
    param (
        $config
    )

    $basePath = $config.installationPath
    $instanceId = $config.instanceId

    $currModulePath = "$basePath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    # Prior to 16.3 the DevShell module was in a different location
    $prevModulePath = "$basePath\Common7\Tools\vsdevshell\Microsoft.VisualStudio.DevShell.dll"

    $modulePath = if (Test-Path $prevModulePath) { $prevModulePath } else { $currModulePath }

    if (Test-Path $modulePath) {
        Write-Verbose "Found at $modulePath."

        try {
            Import-Module $modulePath
        }
        catch [System.IO.FileLoadException] {
            Write-Verbose "The module has already been imported from a different installation of Visual Studio:"
            (Get-Module Microsoft.VisualStudio.DevShell).Path | Write-Verbose
        }

        $params = @{
            VsInstanceId = $instanceId
        }

        $command = Get-Command Enter-VsDevShell

        if ($SkipAutomaticLocation)
        {
            $params.SkipAutomaticLocation = $true
        }

        # -Arch is only available from 17.1
        if ($Arch -and $command.Parameters.ContainsKey("Arch"))
        {
            $params.Arch = $Arch
        }

        # -HostArch is only available from 17.1
        if ($HostArch -and $command.Parameters.ContainsKey("HostArch"))
        {
            $params.HostArch = $HostArch
        }

        # -ReportNewInstanceType is only available from 16.5
        if ($command.Parameters.ContainsKey("ReportNewInstanceType")) {
            $params.ReportNewInstanceType = "LaunchScript"
        }

        $boundParams = $PSCmdlet.MyInvocation.BoundParameters

        if ($boundParams.ContainsKey("Verbose") -and
            $boundParams["Verbose"].IsPresent)
        {
            Write-Verbose "Enter-VsDevShell Parameters:"
            $params.GetEnumerator() | ForEach-Object{
                $message = '{0} = {1}' -f $_.key, $_.value
                Write-Verbose $message
            }
        }

        Enter-VsDevShell @params
        exit
    }

    throw [System.Management.Automation.ErrorRecord]::new(
        [System.Exception]::new("Required assembly could not be located. This most likely indicates an installation error. Try repairing your Visual Studio installation. Expected location: $modulePath"),
        "DevShellModuleLoad",
        [System.Management.Automation.ErrorCategory]::NotInstalled,
        $config)
}

function VsInstallationPath {
    $setupargs = "-path `"$VsInstallationPath`""

    Write-Verbose "Using path: $VsInstallationPath"
    $config = GetSetupConfigurations($setupargs)
    LaunchDevShell($config)
}

function Latest {
    $setupargs = "-latest"

    if (-not $ExcludePrerelease) {
        $setupargs += " -prerelease"
    }

    $config = GetSetupConfigurations($setupargs)
    LaunchDevShell($config)
}

function VsInstanceId {
    $configs = GetSetupConfigurations("-prerelease -all")
    $config = $configs | Where-Object { $_.instanceId -eq $VsInstanceId }
    if ($config) {
        Write-Verbose "Found Visual Studio installation with InstanceId of '$($config.instanceId)' and InstallationPath '$($config.installationPath)'"
        LaunchDevShell($config)
        exit
    }

    throw [System.Management.Automation.ErrorRecord]::new(
        [System.Exception]::new("Could not find an installation of Visual Studio with InstanceId '$VsInstanceId'."),
        "VsSetupInstance",
        [System.Management.Automation.ErrorCategory]::InvalidArgument,
        $config)
}

function List {
    $setupargs = "-sort"

    if (-not $ExcludePrerelease) {
        $setupargs = " -prerelease"
    }

    $configs = GetSetupConfigurations($setupargs)

    $DisplayProperties = @("#") + $DisplayProperties

    # Add an incrementing select column
    $configs = $configs |
        Sort-Object displayName, installationDate |
        ForEach-Object {$i = 0}{ $i++; $_ | Add-Member -NotePropertyName "#" -NotePropertyValue $i -PassThru }

    Write-Host "The following Visual Studio installations were found:"
    $configs | Format-Table -Property $DisplayProperties

    $selected = Read-Host "Enter '#' of the Visual Studio installation to launch DevShell. <Enter> to quit: "
    if (-not $selected) { exit }

    $config = $configs | Where-Object { $_."#" -eq $selected }

    if ($config) {
        LaunchDevShell($config)
    }
    else {
        "Invalid selection: $selected"
    }
}

function Default{
    Write-Verbose "No parameters passed to script. Trying VsInstallationPath."

    try {
        VsInstallationPath
        exit
    }
    catch {
        Write-Verbose "VsInstallationPath failed. Trying Latest."
    }

    Write-Host "Could not start Developer PowerShell using the script path."
    Write-Host "Attempting to launch from the latest Visual Studio installation."

    try {
        Latest
        exit
    }
    catch {
        Write-Verbose "Latest failed. Defaulting to List."
    }

    Write-Host "Could not start Developer PowerShell from the latest Visual Studio installation."
    Write-Host

    List
}

if ($PSCmdlet.ParameterSetName) {
    & (Get-ChildItem "Function:$($PSCmdlet.ParameterSetName)")
    exit
}
# SIG # Begin signature block
# MIInrQYJKoZIhvcNAQcCoIInnjCCJ5oCAQExDzANBglghkgBZQMEAgEFADB5Bgor
# BgEEAYI3AgEEoGswaTA0BgorBgEEAYI3AgEeMCYCAwEAAAQQH8w7YFlLCE63JNLG
# KX7zUQIBAAIBAAIBAAIBAAIBADAxMA0GCWCGSAFlAwQCAQUABCB7cpavZzVzbDmR
# tFRSbdabKCMPm0+YAH01FA8nFbex/KCCDYEwggX/MIID56ADAgECAhMzAAACzI61
# lqa90clOAAAAAALMMA0GCSqGSIb3DQEBCwUAMH4xCzAJBgNVBAYTAlVTMRMwEQYD
# VQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYDVQQKExVNaWNy
# b3NvZnQgQ29ycG9yYXRpb24xKDAmBgNVBAMTH01pY3Jvc29mdCBDb2RlIFNpZ25p
# bmcgUENBIDIwMTEwHhcNMjIwNTEyMjA0NjAxWhcNMjMwNTExMjA0NjAxWjB0MQsw
# CQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9u
# ZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMR4wHAYDVQQDExVNaWNy
# b3NvZnQgQ29ycG9yYXRpb24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIB
# AQCiTbHs68bADvNud97NzcdP0zh0mRr4VpDv68KobjQFybVAuVgiINf9aG2zQtWK
# No6+2X2Ix65KGcBXuZyEi0oBUAAGnIe5O5q/Y0Ij0WwDyMWaVad2Te4r1Eic3HWH
# UfiiNjF0ETHKg3qa7DCyUqwsR9q5SaXuHlYCwM+m59Nl3jKnYnKLLfzhl13wImV9
# DF8N76ANkRyK6BYoc9I6hHF2MCTQYWbQ4fXgzKhgzj4zeabWgfu+ZJCiFLkogvc0
# RVb0x3DtyxMbl/3e45Eu+sn/x6EVwbJZVvtQYcmdGF1yAYht+JnNmWwAxL8MgHMz
# xEcoY1Q1JtstiY3+u3ulGMvhAgMBAAGjggF+MIIBejAfBgNVHSUEGDAWBgorBgEE
# AYI3TAgBBggrBgEFBQcDAzAdBgNVHQ4EFgQUiLhHjTKWzIqVIp+sM2rOHH11rfQw
# UAYDVR0RBEkwR6RFMEMxKTAnBgNVBAsTIE1pY3Jvc29mdCBPcGVyYXRpb25zIFB1
# ZXJ0byBSaWNvMRYwFAYDVQQFEw0yMzAwMTIrNDcwNTI5MB8GA1UdIwQYMBaAFEhu
# ZOVQBdOCqhc3NyK1bajKdQKVMFQGA1UdHwRNMEswSaBHoEWGQ2h0dHA6Ly93d3cu
# bWljcm9zb2Z0LmNvbS9wa2lvcHMvY3JsL01pY0NvZFNpZ1BDQTIwMTFfMjAxMS0w
# Ny0wOC5jcmwwYQYIKwYBBQUHAQEEVTBTMFEGCCsGAQUFBzAChkVodHRwOi8vd3d3
# Lm1pY3Jvc29mdC5jb20vcGtpb3BzL2NlcnRzL01pY0NvZFNpZ1BDQTIwMTFfMjAx
# MS0wNy0wOC5jcnQwDAYDVR0TAQH/BAIwADANBgkqhkiG9w0BAQsFAAOCAgEAeA8D
# sOAHS53MTIHYu8bbXrO6yQtRD6JfyMWeXaLu3Nc8PDnFc1efYq/F3MGx/aiwNbcs
# J2MU7BKNWTP5JQVBA2GNIeR3mScXqnOsv1XqXPvZeISDVWLaBQzceItdIwgo6B13
# vxlkkSYMvB0Dr3Yw7/W9U4Wk5K/RDOnIGvmKqKi3AwyxlV1mpefy729FKaWT7edB
# d3I4+hldMY8sdfDPjWRtJzjMjXZs41OUOwtHccPazjjC7KndzvZHx/0VWL8n0NT/
# 404vftnXKifMZkS4p2sB3oK+6kCcsyWsgS/3eYGw1Fe4MOnin1RhgrW1rHPODJTG
# AUOmW4wc3Q6KKr2zve7sMDZe9tfylonPwhk971rX8qGw6LkrGFv31IJeJSe/aUbG
# dUDPkbrABbVvPElgoj5eP3REqx5jdfkQw7tOdWkhn0jDUh2uQen9Atj3RkJyHuR0
# GUsJVMWFJdkIO/gFwzoOGlHNsmxvpANV86/1qgb1oZXdrURpzJp53MsDaBY/pxOc
# J0Cvg6uWs3kQWgKk5aBzvsX95BzdItHTpVMtVPW4q41XEvbFmUP1n6oL5rdNdrTM
# j/HXMRk1KCksax1Vxo3qv+13cCsZAaQNaIAvt5LvkshZkDZIP//0Hnq7NnWeYR3z
# 4oFiw9N2n3bb9baQWuWPswG0Dq9YT9kb+Cs4qIIwggd6MIIFYqADAgECAgphDpDS
# AAAAAAADMA0GCSqGSIb3DQEBCwUAMIGIMQswCQYDVQQGEwJVUzETMBEGA1UECBMK
# V2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0
# IENvcnBvcmF0aW9uMTIwMAYDVQQDEylNaWNyb3NvZnQgUm9vdCBDZXJ0aWZpY2F0
# ZSBBdXRob3JpdHkgMjAxMTAeFw0xMTA3MDgyMDU5MDlaFw0yNjA3MDgyMTA5MDla
# MH4xCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdS
# ZWRtb25kMR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xKDAmBgNVBAMT
# H01pY3Jvc29mdCBDb2RlIFNpZ25pbmcgUENBIDIwMTEwggIiMA0GCSqGSIb3DQEB
# AQUAA4ICDwAwggIKAoICAQCr8PpyEBwurdhuqoIQTTS68rZYIZ9CGypr6VpQqrgG
# OBoESbp/wwwe3TdrxhLYC/A4wpkGsMg51QEUMULTiQ15ZId+lGAkbK+eSZzpaF7S
# 35tTsgosw6/ZqSuuegmv15ZZymAaBelmdugyUiYSL+erCFDPs0S3XdjELgN1q2jz
# y23zOlyhFvRGuuA4ZKxuZDV4pqBjDy3TQJP4494HDdVceaVJKecNvqATd76UPe/7
# 4ytaEB9NViiienLgEjq3SV7Y7e1DkYPZe7J7hhvZPrGMXeiJT4Qa8qEvWeSQOy2u
# M1jFtz7+MtOzAz2xsq+SOH7SnYAs9U5WkSE1JcM5bmR/U7qcD60ZI4TL9LoDho33
# X/DQUr+MlIe8wCF0JV8YKLbMJyg4JZg5SjbPfLGSrhwjp6lm7GEfauEoSZ1fiOIl
# XdMhSz5SxLVXPyQD8NF6Wy/VI+NwXQ9RRnez+ADhvKwCgl/bwBWzvRvUVUvnOaEP
# 6SNJvBi4RHxF5MHDcnrgcuck379GmcXvwhxX24ON7E1JMKerjt/sW5+v/N2wZuLB
# l4F77dbtS+dJKacTKKanfWeA5opieF+yL4TXV5xcv3coKPHtbcMojyyPQDdPweGF
# RInECUzF1KVDL3SV9274eCBYLBNdYJWaPk8zhNqwiBfenk70lrC8RqBsmNLg1oiM
# CwIDAQABo4IB7TCCAekwEAYJKwYBBAGCNxUBBAMCAQAwHQYDVR0OBBYEFEhuZOVQ
# BdOCqhc3NyK1bajKdQKVMBkGCSsGAQQBgjcUAgQMHgoAUwB1AGIAQwBBMAsGA1Ud
# DwQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB8GA1UdIwQYMBaAFHItOgIxkEO5FAVO
# 4eqnxzHRI4k0MFoGA1UdHwRTMFEwT6BNoEuGSWh0dHA6Ly9jcmwubWljcm9zb2Z0
# LmNvbS9wa2kvY3JsL3Byb2R1Y3RzL01pY1Jvb0NlckF1dDIwMTFfMjAxMV8wM18y
# Mi5jcmwwXgYIKwYBBQUHAQEEUjBQME4GCCsGAQUFBzAChkJodHRwOi8vd3d3Lm1p
# Y3Jvc29mdC5jb20vcGtpL2NlcnRzL01pY1Jvb0NlckF1dDIwMTFfMjAxMV8wM18y
# Mi5jcnQwgZ8GA1UdIASBlzCBlDCBkQYJKwYBBAGCNy4DMIGDMD8GCCsGAQUFBwIB
# FjNodHRwOi8vd3d3Lm1pY3Jvc29mdC5jb20vcGtpb3BzL2RvY3MvcHJpbWFyeWNw
# cy5odG0wQAYIKwYBBQUHAgIwNB4yIB0ATABlAGcAYQBsAF8AcABvAGwAaQBjAHkA
# XwBzAHQAYQB0AGUAbQBlAG4AdAAuIB0wDQYJKoZIhvcNAQELBQADggIBAGfyhqWY
# 4FR5Gi7T2HRnIpsLlhHhY5KZQpZ90nkMkMFlXy4sPvjDctFtg/6+P+gKyju/R6mj
# 82nbY78iNaWXXWWEkH2LRlBV2AySfNIaSxzzPEKLUtCw/WvjPgcuKZvmPRul1LUd
# d5Q54ulkyUQ9eHoj8xN9ppB0g430yyYCRirCihC7pKkFDJvtaPpoLpWgKj8qa1hJ
# Yx8JaW5amJbkg/TAj/NGK978O9C9Ne9uJa7lryft0N3zDq+ZKJeYTQ49C/IIidYf
# wzIY4vDFLc5bnrRJOQrGCsLGra7lstnbFYhRRVg4MnEnGn+x9Cf43iw6IGmYslmJ
# aG5vp7d0w0AFBqYBKig+gj8TTWYLwLNN9eGPfxxvFX1Fp3blQCplo8NdUmKGwx1j
# NpeG39rz+PIWoZon4c2ll9DuXWNB41sHnIc+BncG0QaxdR8UvmFhtfDcxhsEvt9B
# xw4o7t5lL+yX9qFcltgA1qFGvVnzl6UJS0gQmYAf0AApxbGbpT9Fdx41xtKiop96
# eiL6SJUfq/tHI4D1nvi/a7dLl+LrdXga7Oo3mXkYS//WsyNodeav+vyL6wuA6mk7
# r/ww7QRMjt/fdW1jkT3RnVZOT7+AVyKheBEyIXrvQQqxP/uozKRdwaGIm1dxVk5I
# RcBCyZt2WwqASGv9eZ/BvW1taslScxMNelDNMYIZgjCCGX4CAQEwgZUwfjELMAkG
# A1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0b24xEDAOBgNVBAcTB1JlZG1vbmQx
# HjAcBgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3JhdGlvbjEoMCYGA1UEAxMfTWljcm9z
# b2Z0IENvZGUgU2lnbmluZyBQQ0EgMjAxMQITMwAAAsyOtZamvdHJTgAAAAACzDAN
# BglghkgBZQMEAgEFAKCBrjAZBgkqhkiG9w0BCQMxDAYKKwYBBAGCNwIBBDAcBgor
# BgEEAYI3AgELMQ4wDAYKKwYBBAGCNwIBFTAvBgkqhkiG9w0BCQQxIgQgc+xnwNto
# Cneb1M+6ybu+WiyvAGf6j7BbsfjtJAuNGnkwQgYKKwYBBAGCNwIBDDE0MDKgFIAS
# AE0AaQBjAHIAbwBzAG8AZgB0oRqAGGh0dHA6Ly93d3cubWljcm9zb2Z0LmNvbTAN
# BgkqhkiG9w0BAQEFAASCAQAXE1PEUYBZdCQ2/0kGFwOJXVxBVdRixoW4ZvXLJHoR
# IakDUd7uF/MWz4oJrB+5Etz6GVcsUJJ1yd3BDKaSw8BezhvgPxp2QbyZfHEETzlO
# Nl0sQ1DVO9YLiXa3dwS4NavRgeUnCgP0rQ3EKx6orpcd8QU4d8XOOIWQ0SrQJaUZ
# 7GrQBQwCDHVDPsXpDiCClK8JT4eydteqzEiisrhpvHBTiJkB+al3+nArJYEhGvLo
# ORGFvNfVi9r7PVmKRnJMnhVWlbear1wxXlNBZearaNbGEoQeQ3saS9BWpUX5snCn
# IxUp4gk/No6zT1SxINPIzKwSfvaBplnzLWOQbqk8LdypoYIXDDCCFwgGCisGAQQB
# gjcDAwExghb4MIIW9AYJKoZIhvcNAQcCoIIW5TCCFuECAQMxDzANBglghkgBZQME
# AgEFADCCAVUGCyqGSIb3DQEJEAEEoIIBRASCAUAwggE8AgEBBgorBgEEAYRZCgMB
# MDEwDQYJYIZIAWUDBAIBBQAEIOMfX8L/yGRXkvIxOaA/EBL87Rzq7uWmFvQTmrEl
# MQPfAgZjKfP/EsQYEzIwMjIxMDE1MjE1NTQyLjc5OFowBIACAfSggdSkgdEwgc4x
# CzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRt
# b25kMR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xKTAnBgNVBAsTIE1p
# Y3Jvc29mdCBPcGVyYXRpb25zIFB1ZXJ0byBSaWNvMSYwJAYDVQQLEx1UaGFsZXMg
# VFNTIEVTTjo4OTdBLUUzNTYtMTcwMTElMCMGA1UEAxMcTWljcm9zb2Z0IFRpbWUt
# U3RhbXAgU2VydmljZaCCEV8wggcQMIIE+KADAgECAhMzAAABqwkJ76tj1OipAAEA
# AAGrMA0GCSqGSIb3DQEBCwUAMHwxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpXYXNo
# aW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYDVQQKExVNaWNyb3NvZnQgQ29y
# cG9yYXRpb24xJjAkBgNVBAMTHU1pY3Jvc29mdCBUaW1lLVN0YW1wIFBDQSAyMDEw
# MB4XDTIyMDMwMjE4NTEyOFoXDTIzMDUxMTE4NTEyOFowgc4xCzAJBgNVBAYTAlVT
# MRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYDVQQK
# ExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xKTAnBgNVBAsTIE1pY3Jvc29mdCBPcGVy
# YXRpb25zIFB1ZXJ0byBSaWNvMSYwJAYDVQQLEx1UaGFsZXMgVFNTIEVTTjo4OTdB
# LUUzNTYtMTcwMTElMCMGA1UEAxMcTWljcm9zb2Z0IFRpbWUtU3RhbXAgU2Vydmlj
# ZTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAMmdS1o5dehASUsscLqy
# x2wm/WirNUfqkGBymDItYzEnoKtkhrd7wNsJs4g+BuM3uBX81WnO270lkrC0e1mm
# DqQt420Tmb8lwsjQKM6mEaNQIfXDronrVN3aw1lx9bAf7VZEA3kHFql6YAO3kjQ6
# PftA4iVHX3JVv98ntjkbtqzKeJMaNWd8dBaAD3RCliMoajTDGbyYNKTvxBhWILyJ
# 8WYdJ/NBDpqPzQl+pxm6ZZVSeBQAIOubZjU0vfpECxHC5vI1ErrqapG+0oBhhON+
# gllVklPAWZv2iv0mgjCTj7YNKX7yL2x2TvrvHVq5GPNa5fNbpy39t5cviiYqMf1R
# ZVZccdr+2vApk5ib5a4O8SiAgPSUwYGoOwbZG1onHij0ATPLkgKUfgaPzFfd5JZS
# bRl2Xg347/LjWQLR+KjAyACFb06bqWzvHtQJTND8Y0j5Y2SBnSCqV2zNHSVts4+a
# UfkUhsKS+GAXS3j5XUgYA7SMNog76Nnss5l01nEX7sHDdYykYhzuQKFrT70XVTZe
# X25tSBfy3VaczYd1JSI/9wOGqbFU52NyrlsA1qimxOhsuds7Pxo+jO3RjV/kC+AE
# OoVaXDdminsc3PtlBCVh/sgYno9AUymblSRmee1gwlnlZJ0uiHKI9q2HFgZWM10y
# PG5gVt0prXnJFi1Wxmmg+BH/AgMBAAGjggE2MIIBMjAdBgNVHQ4EFgQUFFvO8o1e
# NcSCIQZMvqGfdNL+pqowHwYDVR0jBBgwFoAUn6cVXQBeYl2D9OXSZacbUzUZ6XIw
# XwYDVR0fBFgwVjBUoFKgUIZOaHR0cDovL3d3dy5taWNyb3NvZnQuY29tL3BraW9w
# cy9jcmwvTWljcm9zb2Z0JTIwVGltZS1TdGFtcCUyMFBDQSUyMDIwMTAoMSkuY3Js
# MGwGCCsGAQUFBwEBBGAwXjBcBggrBgEFBQcwAoZQaHR0cDovL3d3dy5taWNyb3Nv
# ZnQuY29tL3BraW9wcy9jZXJ0cy9NaWNyb3NvZnQlMjBUaW1lLVN0YW1wJTIwUENB
# JTIwMjAxMCgxKS5jcnQwDAYDVR0TAQH/BAIwADATBgNVHSUEDDAKBggrBgEFBQcD
# CDANBgkqhkiG9w0BAQsFAAOCAgEAykuUgTc1KMszMgsHbhgjgEGv/dCHFf0by99C
# 45SR770/udCNNeqlT610Ehz13xGFU6Hci+TLUPUnhvUnSuz7xkiWRru5RjZZmSon
# EVv8npa3z1QveUfngtyi0Jd6qlSykoEVJ6tDuR1Kw9xU9yvthZWhQs/ymyOwh+mx
# t0C9wbeLJ92er2vc9ly12pFxbCNDJ+mQ7v520hAvreWqZ02GOJhw0R4c1iP39iNB
# zHOoz+DsO0sYjwhaz9HrvYMEzOD1MJdLPWfUFsZ//iTd3jzEykk02WjnZNzIe2EN
# fmQ/KblGXHeSe8JYqimTFxl5keMfLUELjAh0mhQ1vLCJZ20BwC4O57Eg7yO/YuBn
# o+4RrV0CD2gp4BO10KFW2SQ/MhvRWK7HbgS6Bzt70rkIeSUto7pRkHMqrnhubITc
# Xddky6GtZsmwM3hvqXuStMeU1W5NN3HA8ypjPLd/bomfGx96Huw8OrftcQvk7thd
# Nu4JhAyKUXUP7dKMCJfrOdplg0j1tE0aiE+pDTSQVmPzGezCL42slyPJVXpu4xxE
# 0hpACr2ua0LHv/LB6RV5C4CO4Ms/pfal//F3O+hJZe5ixevzKNkXXbxPOa1R+SIr
# W/rHZM6RIDLTJxTGFDM1hQDyafGu9S/a7umkvilgBHNxZfk0IYE7RRWJcG7oiY+F
# Gdx1cs0wggdxMIIFWaADAgECAhMzAAAAFcXna54Cm0mZAAAAAAAVMA0GCSqGSIb3
# DQEBCwUAMIGIMQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4G
# A1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMTIw
# MAYDVQQDEylNaWNyb3NvZnQgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgMjAx
# MDAeFw0yMTA5MzAxODIyMjVaFw0zMDA5MzAxODMyMjVaMHwxCzAJBgNVBAYTAlVT
# MRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYDVQQK
# ExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xJjAkBgNVBAMTHU1pY3Jvc29mdCBUaW1l
# LVN0YW1wIFBDQSAyMDEwMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA
# 5OGmTOe0ciELeaLL1yR5vQ7VgtP97pwHB9KpbE51yMo1V/YBf2xK4OK9uT4XYDP/
# XE/HZveVU3Fa4n5KWv64NmeFRiMMtY0Tz3cywBAY6GB9alKDRLemjkZrBxTzxXb1
# hlDcwUTIcVxRMTegCjhuje3XD9gmU3w5YQJ6xKr9cmmvHaus9ja+NSZk2pg7uhp7
# M62AW36MEBydUv626GIl3GoPz130/o5Tz9bshVZN7928jaTjkY+yOSxRnOlwaQ3K
# Ni1wjjHINSi947SHJMPgyY9+tVSP3PoFVZhtaDuaRr3tpK56KTesy+uDRedGbsoy
# 1cCGMFxPLOJiss254o2I5JasAUq7vnGpF1tnYN74kpEeHT39IM9zfUGaRnXNxF80
# 3RKJ1v2lIH1+/NmeRd+2ci/bfV+AutuqfjbsNkz2K26oElHovwUDo9Fzpk03dJQc
# NIIP8BDyt0cY7afomXw/TNuvXsLz1dhzPUNOwTM5TI4CvEJoLhDqhFFG4tG9ahha
# YQFzymeiXtcodgLiMxhy16cg8ML6EgrXY28MyTZki1ugpoMhXV8wdJGUlNi5UPkL
# iWHzNgY1GIRH29wb0f2y1BzFa/ZcUlFdEtsluq9QBXpsxREdcu+N+VLEhReTwDwV
# 2xo3xwgVGD94q0W29R6HXtqPnhZyacaue7e3PmriLq0CAwEAAaOCAd0wggHZMBIG
# CSsGAQQBgjcVAQQFAgMBAAEwIwYJKwYBBAGCNxUCBBYEFCqnUv5kxJq+gpE8RjUp
# zxD/LwTuMB0GA1UdDgQWBBSfpxVdAF5iXYP05dJlpxtTNRnpcjBcBgNVHSAEVTBT
# MFEGDCsGAQQBgjdMg30BATBBMD8GCCsGAQUFBwIBFjNodHRwOi8vd3d3Lm1pY3Jv
# c29mdC5jb20vcGtpb3BzL0RvY3MvUmVwb3NpdG9yeS5odG0wEwYDVR0lBAwwCgYI
# KwYBBQUHAwgwGQYJKwYBBAGCNxQCBAweCgBTAHUAYgBDAEEwCwYDVR0PBAQDAgGG
# MA8GA1UdEwEB/wQFMAMBAf8wHwYDVR0jBBgwFoAU1fZWy4/oolxiaNE9lJBb186a
# GMQwVgYDVR0fBE8wTTBLoEmgR4ZFaHR0cDovL2NybC5taWNyb3NvZnQuY29tL3Br
# aS9jcmwvcHJvZHVjdHMvTWljUm9vQ2VyQXV0XzIwMTAtMDYtMjMuY3JsMFoGCCsG
# AQUFBwEBBE4wTDBKBggrBgEFBQcwAoY+aHR0cDovL3d3dy5taWNyb3NvZnQuY29t
# L3BraS9jZXJ0cy9NaWNSb29DZXJBdXRfMjAxMC0wNi0yMy5jcnQwDQYJKoZIhvcN
# AQELBQADggIBAJ1VffwqreEsH2cBMSRb4Z5yS/ypb+pcFLY+TkdkeLEGk5c9MTO1
# OdfCcTY/2mRsfNB1OW27DzHkwo/7bNGhlBgi7ulmZzpTTd2YurYeeNg2LpypglYA
# A7AFvonoaeC6Ce5732pvvinLbtg/SHUB2RjebYIM9W0jVOR4U3UkV7ndn/OOPcbz
# aN9l9qRWqveVtihVJ9AkvUCgvxm2EhIRXT0n4ECWOKz3+SmJw7wXsFSFQrP8DJ6L
# GYnn8AtqgcKBGUIZUnWKNsIdw2FzLixre24/LAl4FOmRsqlb30mjdAy87JGA0j3m
# Sj5mO0+7hvoyGtmW9I/2kQH2zsZ0/fZMcm8Qq3UwxTSwethQ/gpY3UA8x1RtnWN0
# SCyxTkctwRQEcb9k+SS+c23Kjgm9swFXSVRk2XPXfx5bRAGOWhmRaw2fpCjcZxko
# JLo4S5pu+yFUa2pFEUep8beuyOiJXk+d0tBMdrVXVAmxaQFEfnyhYWxz/gq77EFm
# PWn9y8FBSX5+k77L+DvktxW/tM4+pTFRhLy/AsGConsXHRWJjXD+57XQKBqJC482
# 2rpM+Zv/Cuk0+CQ1ZyvgDbjmjJnW4SLq8CdCPSWU5nR0W2rRnj7tfqAxM328y+l7
# vzhwRNGQ8cirOoo6CGJ/2XBjU02N7oJtpQUQwXEGahC0HVUzWLOhcGbyoYIC0jCC
# AjsCAQEwgfyhgdSkgdEwgc4xCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpXYXNoaW5n
# dG9uMRAwDgYDVQQHEwdSZWRtb25kMR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9y
# YXRpb24xKTAnBgNVBAsTIE1pY3Jvc29mdCBPcGVyYXRpb25zIFB1ZXJ0byBSaWNv
# MSYwJAYDVQQLEx1UaGFsZXMgVFNTIEVTTjo4OTdBLUUzNTYtMTcwMTElMCMGA1UE
# AxMcTWljcm9zb2Z0IFRpbWUtU3RhbXAgU2VydmljZaIjCgEBMAcGBSsOAwIaAxUA
# W6h6/24WCo7WZz6CEVAeLztcmD6ggYMwgYCkfjB8MQswCQYDVQQGEwJVUzETMBEG
# A1UECBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWlj
# cm9zb2Z0IENvcnBvcmF0aW9uMSYwJAYDVQQDEx1NaWNyb3NvZnQgVGltZS1TdGFt
# cCBQQ0EgMjAxMDANBgkqhkiG9w0BAQUFAAIFAOb1ZwAwIhgPMjAyMjEwMTUyMTA2
# MDhaGA8yMDIyMTAxNjIxMDYwOFowdzA9BgorBgEEAYRZCgQBMS8wLTAKAgUA5vVn
# AAIBADAKAgEAAgIHjQIB/zAHAgEAAgITxTAKAgUA5va4gAIBADA2BgorBgEEAYRZ
# CgQCMSgwJjAMBgorBgEEAYRZCgMCoAowCAIBAAIDB6EgoQowCAIBAAIDAYagMA0G
# CSqGSIb3DQEBBQUAA4GBAJO3r5f1YgyxFsK4CNkx7sRpCsJdHYvTfCFz83NTyB2l
# 9Z3FPwu/N/5aijINi0JbFEcd5nkXG6F96ao9JB343ZJouqmRNY+SqIGsB3buUO0+
# H9tD1ByCc53nTLBNFoI670cOTFv2WWm/GHWhR/6U6kK/1CKQxi3S32LY9syt+SJ9
# MYIEDTCCBAkCAQEwgZMwfDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0
# b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3Jh
# dGlvbjEmMCQGA1UEAxMdTWljcm9zb2Z0IFRpbWUtU3RhbXAgUENBIDIwMTACEzMA
# AAGrCQnvq2PU6KkAAQAAAaswDQYJYIZIAWUDBAIBBQCgggFKMBoGCSqGSIb3DQEJ
# AzENBgsqhkiG9w0BCRABBDAvBgkqhkiG9w0BCQQxIgQgPLnHTyKnIuw2MX09/CMd
# IQq3RP9fPIS5/tao4rMRI18wgfoGCyqGSIb3DQEJEAIvMYHqMIHnMIHkMIG9BCAO
# HK/6sIVgEVSVD3Arvk6OyQKvRxFHKyraUzbN1/AKVzCBmDCBgKR+MHwxCzAJBgNV
# BAYTAlVTMRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdSZWRtb25kMR4w
# HAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xJjAkBgNVBAMTHU1pY3Jvc29m
# dCBUaW1lLVN0YW1wIFBDQSAyMDEwAhMzAAABqwkJ76tj1OipAAEAAAGrMCIEILkO
# Q+SuFSky1Qb9DGaWNNaxjHfO9JFy7/7dF98S8YzjMA0GCSqGSIb3DQEBCwUABIIC
# AABmrCIT3sPWc1zbZsLAOaVa2n6sH7whgKA+E6GS9arK3xIyB0srx/BubNDDljpx
# oW0SXSaH/fzKP/Qznc1C5HYueSCNCvqQfpukbe4QhHRIO9AcGZ0h3EtYBhIEvRtu
# lKbx5z2xPabrUQuMeBWXwtLvJ8NVJRXvoqplFPlboFnRjuWoxT99Ll7fzUeT0jZf
# NuHC3DGL09lx2sbvAWdEO5lqaZTCWe0rn5G25UZAnJwz0yVkLxTmdoFw8ALCKSlV
# Gx89bJSYreP2Eo1Vfa6KuDJts+fwTCjHVUA0WAM7uMSg+INCXw1FnG3Jx9fvgXwe
# ySRl4X8NF/QcSuS3Pag128dN7GaN4QfA7T4vgAg6DkWEPs95PPeLnZJnyP10frrC
# JQQL3dJMfjPQZtj3s1/KLKVPkviB7ZPXc9EXwtir0I4+RHxiJ8UNZJh+LbQP5PGb
# ocrurKw0DLwy4bIliEGCOZwiAEf1aWGiWhFyN6Ztr8ydE6UFDz0w7oiCn0tFWN+H
# rzqoeNSNCtpqfm3SYDCHBW/w6+nUvJzuREQwGs6p79+dxMUxrZ6HM6mSzsji5xAc
# O5MXglQdSb+9IbB0VUgSfdCoopaUSHxJZIP9ZbejbQJGI/2T7scxnR69y+lPmKv7
# HcDEhMDGF7j7nkis5EgJoYKXPpG+SajQayGN1jyTrzEF
# SIG # End signature block
