# Code Signing — z80cpmw beta packages

How the z80cpmw Windows builds get an Authenticode signature so testers don't see
an "unknown publisher" warning when they install a beta build.

We use **Azure Artifact Signing** (the service formerly called *Trusted Signing* /
*Azure Code Signing*). The publisher identity is an **individual** developer
(legal name **Aaron Wohl**), validated for **Public Trust**, which is the trust
level that makes the signature valid on any normal Windows machine.

> **SmartScreen reputation caveat.** A correctly signed binary removes the
> *unknown-publisher* warning, but Microsoft SmartScreen reputation accrues over
> time per publisher. Brand-new certificates may still trigger a "Windows
> protected your PC" prompt on early downloads even though the signature is valid.
> It clears as download volume builds reputation. This is expected and not a
> setup error.

---

## What needs signing

The beta vehicle is the **signed MSIX sideload package**
`dist\z80cpmw-<version>-beta.msix`, produced and signed in one step by
`build-msix.ps1 -Beta` (see the two-vehicle policy below). The NSIS installer path
(`packaging/scripts/build-nsis.ps1`) is retained for direct distribution but has
not been released since v1.0.14; if it is revived, both of its PE files need
signing:

- `bin\Release\z80cpmw.exe` — the application binary. Sign it **before** NSIS
  packaging so the installed program is itself signed.
- `dist\z80cpmw-<version>-setup.exe` — the NSIS installer testers download. The
  version in the name comes from `z80cpmw\Version.h`, which `build-nsis.ps1`
  parses. Sign it **after** `makensis` builds it.

The **MSIX** package (`packaging/msix/`) follows a two-vehicle policy:

- **Normal Store releases** keep the Store identity in the manifest
  (`Publisher="CN=724C9014-…"`) and are **re-signed by Microsoft** at submission —
  you don't sign them yourself. This is the default `build-msix.ps1` output.
- **Beta builds** are distributed for **sideloading**, so they're signed here with
  our Azure Artifact Signing cert. Run `build-msix.ps1 -Beta`: it emits
  `dist\z80cpmw-<ver>-beta.msix`, signs it (`signtool` + dlib, below) and verifies.
- **Beta rehearsals** (`build-msix.ps1 -Beta -SkipSign`) run the beta vehicle with
  the signing call removed and emit `dist\z80cpmw-<ver>-beta-unsigned.msix` plus its
  `.pdb`. Nothing about this output is publishable — it is how you check the beta
  path without spending a signing call. See the two-stage recipe below.

> **MSIX publisher gotcha.** `signtool` requires the package's `<Identity Publisher>`
> to **exactly equal the signing cert subject** (`CN=Aaron Wohl, O=Aaron Wohl,
> L=Gainesville, S=fl, C=US`). The committed manifest carries the *Store* identity
> GUID, which the Aaron Wohl cert cannot sign — so `-Beta` rewrites the Publisher to
> the cert subject in a staged manifest copy before packing (the committed
> `AppxManifest.xml` is left untouched for Store submission). A beta build therefore
> has a different package identity and installs side-by-side with a Store install.
> The two channels can carry the *same* version number when they are the same
> build — Store **1.0.22** and `dist\z80cpmw-1.0.22-beta.msix` hold a
> byte-identical `z80cpmw.exe` — because it is the Publisher, not the number, that
> separates the identities. Where the builds differ, the numbers must differ too.

### Build a beta in two stages: rehearse, then sign

A signed beta run has two effects that cannot be taken back. It spends a real
Azure Trusted Signing call, and it writes `dist\z80cpmw-<ver>-beta.msix` — the
exact name under which a beta gets published. Run it unsigned first.

**Stage 1 — the rehearsal.** Safe to run at any time, on any version:

```powershell
.\build-msix.ps1 -Beta -SkipBuild -SkipSign
```

It does everything the signed run does except sign: version check, Publisher
rewrite, `makeappx pack`, and the symbol copy. Both outputs carry `-unsigned` in
the name, so the rehearsal cannot overwrite or be mistaken for a shipped package.
Confirm `dist\z80cpmw-<ver>-beta-unsigned.msix` **and**
`dist\z80cpmw-<ver>-beta-unsigned.pdb` both appear, then delete both — they are
build output, and a stray unsigned package in `dist\` is exactly the thing that
gets attached to a release by accident. Drop `-SkipBuild` if `bin\Release` is not
already the build you mean to package.

**Stage 2 — the real thing.** Only once stage 1 is clean, **and only for a version
that has not already shipped**:

```powershell
.\build-msix.ps1 -Beta
```

Check `z80cpmw\Version.h` against the published releases before running this. A
beta whose version is already on GitHub will be silently re-minted from whatever
is in `bin\Release` — the version guard compares `bin\Release\z80cpmw.exe` to
`Version.h` and says nothing about the artifact being replaced, so two different
binaries can carry one version number and pass. Bump the version instead.

Do not reach for `-WhatIf`: the script does not implement it and rejects it, on
purpose. A `-WhatIf` that skipped only the steps someone remembered to guard
would be more dangerous than none. `-SkipSign` is the real dry run.

---

## Azure resources (already provisioned)

Non-secret operational values — safe to keep in this public repo:

- Service: Azure Artifact Signing (formerly Trusted Signing)
- Signing account: `ms-code-sign-account`
- Resource group: `ms-code-signing`
- Region: East US
- Data-plane endpoint: `https://eus.codesigning.azure.net/`
- Identity / trust type: **Individual → Public Trust**
- Certificate subject (CN): legal name from the Azure billing profile (**Aaron Wohl**)
- Certificate profile name: `z80cpmw-public` *(created; in daily use — see the
  signing kit's `metadata.json`)*

Roles required and assigned (on the signing-account scope):

- `Artifact Signing Identity Verifier` — needed to create the identity validation.
- `Artifact Signing Certificate Profile Signer` — needed to actually sign.

Both roles are assigned to the human Azure account **and** to the automation
service principal. Neither role is inherited from Owner/Contributor — they must be
granted explicitly.

> **Secrets are NOT in this repo.** The tenant ID, the service principal's app ID
> and (especially) its client secret live only on the build machine, in the signing
> kit at `C:\temp\in\z80cpmw-signing-kit\credentials.ps1` — outside the repo tree.
> Override the kit location with `$env:Z80CPMW_SIGNING_KIT` or
> `build-msix.ps1 -SigningKit <dir>`. Never commit that file or paste the secret
> anywhere. The SP secret expires ~1 year after creation (created 2026-06-24) —
> rotate before then.

---

## One-time setup status

1. [x] Provider `Microsoft.CodeSigning` registered.
2. [x] Signing account + resource group created.
3. [x] Both signing roles assigned (human account + automation SP).
4. [x] **Identity validation** — Individual → Public, **Completed** in the Azure
   portal. *Manual, cannot be scripted; requires a government photo ID via the
   Microsoft Authenticator app. One-time per identity, reusable across profiles.*
5. [x] **Certificate profile** `z80cpmw-public` (Public Trust) created — in use
   today: the signing kit's `metadata.json` names it, and
   `dist\z80cpmw-1.0.22-beta.msix` was signed and timestamped through it.

Setup is complete and signing works today: `build-msix.ps1 -Beta` produces a
signed, timestamped sideload MSIX through the signing kit, with no manual
credential step. The `az` command below is kept as the record of how the profile
was created, and as the recipe if it ever has to be recreated.

### How `z80cpmw-public` was created (historical — the profile already exists; do not re-run)

Get the `identityValidationId` from the portal (the completed Individual/Public
validation), then:

```bash
az extension add --name trustedsigning           # one-time
az trustedsigning certificate-profile create \
  --resource-group ms-code-signing \
  --account-name ms-code-sign-account \
  --name z80cpmw-public \
  --profile-type PublicTrust \
  --identity-validation-id <IDENTITY_VALIDATION_ID>
```

Use `--profile-type PublicTrustTest` only for *inner-loop* pipeline tests — those
certs are **not** publicly trusted and will still warn on testers' machines. For
beta packages that leave your machine, use `PublicTrust`.

---

## How to sign

Three options. Pick by where you build/release. All authenticate to Azure and all
require the `Certificate Profile Signer` role (already granted).

Azure Artifact Signing certificates are only valid for **3 days**, so every
signature **must be timestamped** for long-term validity. The Trusted Signing
timestamp authority is `http://timestamp.acs.microsoft.com`. `jsign` and the MS
tooling enable timestamping automatically; with raw `signtool` you pass it
explicitly (shown below).

### Authentication

The signer authenticates as either your interactive `az login` or the automation
service principal. The SP's credentials live in the signing kit as a PowerShell
script — there is no POSIX-sourceable env file anywhere:

```powershell
. "C:\temp\in\z80cpmw-signing-kit\credentials.ps1"   # sets AZURE_TENANT_ID / AZURE_CLIENT_ID / AZURE_CLIENT_SECRET
```

That file sets no subscription ID, and nothing here needs one. The kit's
`sign.ps1` dot-sources `credentials.ps1` itself, so the normal
`build-msix.ps1 -Beta` flow needs no manual credential step at all — the line
above is only for signing something by hand.

The Microsoft signtool/dotnet-sign tooling also reads those same `AZURE_*` env
vars directly via DefaultAzureCredential, so no explicit `az login` is needed once
they are set. For an `az` session under the same identity:

```powershell
az login --service-principal -u "$env:AZURE_CLIENT_ID" -p "$env:AZURE_CLIENT_SECRET" -t "$env:AZURE_TENANT_ID"
```

### Option 1 — Windows: `signtool` + Trusted Signing dlib  *(integrates with the existing build scripts)*

This is the most native path for this project because the build already runs on
Windows (MSBuild + the PowerShell packaging scripts), and `build-msix.ps1` already
drives `signtool` (through the signing kit) for `-Beta`.

1. Install the dlib (one-time). Either the NuGet package
   `Microsoft.Trusted.Signing.Client` (gives `bin\x64\Azure.CodeSigning.Dlib.dll`)
   or the PowerShell module:

   ```powershell
   Install-Module -Name TrustedSigning -Scope CurrentUser
   ```

   The signing kit already bundles the dlib in `dlib\`, so this is only needed on
   a build machine that does not have the kit.

2. Create a metadata file `trusted-signing.json` next to the build:

   ```json
   {
     "Endpoint": "https://eus.codesigning.azure.net/",
     "CodeSigningAccountName": "ms-code-sign-account",
     "CertificateProfileName": "z80cpmw-public"
   }
   ```

   This is exactly the kit's `metadata.json`.

3. Sign — one invocation per file. This is the command shape the kit's `sign.ps1`
   runs:

   ```bat
   signtool.exe sign /v /fd SHA256 ^
     /tr "http://timestamp.acs.microsoft.com" /td SHA256 ^
     /dlib "<path>\Azure.CodeSigning.Dlib.dll" ^
     /dmdf "<path>\trusted-signing.json" ^
     "dist\z80cpmw-1.0.22-beta.msix"
   ```

Where to hook it into the existing scripts:

- `packaging/scripts/build-nsis.ps1` (not currently wired up): sign
  `bin\Release\z80cpmw.exe` right after the MSBuild step (Step 1), and sign the
  moved `dist\...-setup.exe` at the end (after Step 5).
- `packaging/scripts/build-msix.ps1`: the `-Beta` switch already wires this in — it
  rewrites the manifest Publisher to the cert subject, packs, then calls the signing
  kit's `sign.ps1` (signtool + dlib) and verifies. The kit folder defaults to
  `$env:Z80CPMW_SIGNING_KIT` (else `C:\temp\in\z80cpmw-signing-kit`); override with
  `-SigningKit <dir>`. Adding `-SkipSign` takes the rehearsal arm, which never
  computes a path into the kit at all, so the rehearsal is safe on a machine where
  the kit is present and the credentials work. The legacy `-CertificatePath`
  (`.pfx`) path is retained only for local self-signed testing.

### Option 2 — `dotnet sign` (Windows convenience wrapper)

```powershell
dotnet tool install --global sign --prerelease
sign code trusted-signing "bin\Release\z80cpmw.exe" `
  --trusted-signing-endpoint https://eus.codesigning.azure.net/ `
  --trusted-signing-account ms-code-sign-account `
  --trusted-signing-certificate-profile z80cpmw-public
```

Notes:
- Requires the .NET SDK.
- The tool is mid-rename from `trusted-signing` to `artifact-signing`; if the
  subcommand/flags above are rejected, run `sign code --help` for the current
  names.
- **Authenticode signing in `dotnet sign` only works on Windows** — it cannot
  sign a Windows `.exe`/`.dll` from Linux/macOS. For Linux use Option 3.

### Option 3 — `jsign` (cross-platform: Linux build box or CI)

`jsign` is a Java Authenticode signer that supports Artifact/Trusted Signing and
runs anywhere, so it can sign the app exe (and the NSIS installer, if that path is
revived) from the Linux build box or a Linux CI runner. It does **not** sign MSIX,
which is the current beta vehicle — so this option cannot produce a beta package
on its own.

```bash
# Install (Ubuntu): download the .deb from https://github.com/ebourg/jsign/releases
#   sudo apt install ./jsign_<ver>_all.deb        # needs a Java 8+ runtime
# or run the standalone jar: java -jar jsign-<ver>.jar ...

# There is no env file to source on Linux: copy AZURE_TENANT_ID / AZURE_CLIENT_ID /
# AZURE_CLIENT_SECRET out of the signing kit's credentials.ps1 and export them.
export AZURE_TENANT_ID=... AZURE_CLIENT_ID=... AZURE_CLIENT_SECRET=...
az login --service-principal -u "$AZURE_CLIENT_ID" -p "$AZURE_CLIENT_SECRET" -t "$AZURE_TENANT_ID"
TOKEN=$(az account get-access-token --resource https://codesigning.azure.net --query accessToken -o tsv)

jsign \
  --storetype TRUSTEDSIGNING \
  --keystore  https://eus.codesigning.azure.net \
  --storepass "$TOKEN" \
  --alias     "ms-code-sign-account/z80cpmw-public" \
  bin/Release/z80cpmw.exe
```

> **No trailing slash on `--keystore`.** jsign concatenates the path onto this
> URL, so `https://eus.codesigning.azure.net/` produces a `//codesigningaccounts`
> double slash and the sign call 404s. Use `https://eus.codesigning.azure.net`.

`jsign` timestamps automatically with this storetype. The access token is
short-lived (~1 hour), so fetch it right before signing.

A command of this shape has been verified end-to-end from Linux: it signs a real
z80cpmw PE with the Public Trust cert (subject `CN=Aaron Wohl, …`) and an embedded
Microsoft RFC-3161 timestamp.

---

## Verify a signature

- Windows: `signtool verify /pa /v "dist\z80cpmw-1.0.22-beta.msix"`, or
  `.\sign.ps1 -Verify <file>` from the signing kit, or right-click the file →
  **Properties → Digital Signatures**.
- Cross-platform, PE files only — `osslsigncode` cannot read an MSIX:
  `osslsigncode verify <a signed .exe>`. Nothing in this tree currently qualifies:
  `bin\Release\z80cpmw.exe` is unsigned (the exe is signed only inside the signed
  MSIX), and the packages in `dist\` are MSIX.

> On Linux, `osslsigncode verify` exits non-zero with "unable to get local issuer
> certificate" because the box lacks Microsoft's root CAs — this is a local
> trust-store gap, **not** a bad signature. Check that `Current message digest`
> equals `Calculated message digest` and that the chain ends at *Microsoft
> Identity Verification Root Certificate Authority 2020*. On Windows (where that
> root is trusted) the signature verifies fully.

A good signature shows the subject/publisher as the validated legal name and a
present, valid countersignature (timestamp).

---

## Security checklist

- Never commit the signing kit (`C:\temp\in\z80cpmw-signing-kit\`, especially
  `credentials.ps1`) or any client secret / access token. Keep the kit outside the
  repo tree.
- Keep the SP scoped and rotate its secret before it expires (~2027-06-24).
- Prefer signing with the SP (`Certificate Profile Signer` role) over broad
  credentials.
- Always timestamp (the 3-day cert lifetime makes it mandatory for durable
  signatures).

---

## References

- Azure Artifact Signing docs: https://learn.microsoft.com/en-us/azure/artifact-signing/
- Trust models (Public vs Private): https://learn.microsoft.com/en-us/azure/artifact-signing/concept-trust-models
- Set up signing integrations (signtool/dlib, dotnet sign): https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations
- `dotnet sign`: https://github.com/dotnet/sign
- `jsign` (cross-platform): https://ebourg.github.io/jsign/
