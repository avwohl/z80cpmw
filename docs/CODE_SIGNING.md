# Code Signing — z80cpmw beta installers

How the z80cpmw Windows builds get an Authenticode signature so testers don't see
an "unknown publisher" warning when they run a beta installer.

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

The beta vehicle is the **NSIS installer**, plus the app binary inside it. Both
are PE files and both should be signed:

- `bin\Release\z80cpmw.exe` — the application binary. Sign it **before** NSIS
  packaging so the installed program is itself signed.
- `dist\z80cpmw-<version>-setup.exe` — the NSIS installer testers download
  (e.g. `z80cpmw-1.0.17-setup.exe`). Sign it **after** `makensis` builds it.

The **MSIX** package (`packaging/msix/`) follows a two-vehicle policy:

- **Normal Store releases** keep the Store identity in the manifest
  (`Publisher="CN=724C9014-…"`) and are **re-signed by Microsoft** at submission —
  you don't sign them yourself. This is the default `build-msix.ps1` output.
- **Beta builds** are distributed for **sideloading**, so they're signed here with
  our Azure Artifact Signing cert. Run `build-msix.ps1 -Beta`: it emits
  `dist\z80cpmw-<ver>-beta.msix`, signs it (`signtool` + dlib, below) and verifies.

> **MSIX publisher gotcha.** `signtool` requires the package's `<Identity Publisher>`
> to **exactly equal the signing cert subject** (`CN=Aaron Wohl, O=Aaron Wohl,
> L=Gainesville, S=fl, C=US`). The committed manifest carries the *Store* identity
> GUID, which the Aaron Wohl cert cannot sign — so `-Beta` rewrites the Publisher to
> the cert subject in a staged manifest copy before packing (the committed
> `AppxManifest.xml` is left untouched for Store submission). A beta build therefore
> has a different package identity and installs side-by-side with a Store install.

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
- Certificate profile name: `z80cpmw-public` *(planned — created in Step C below)*

Roles required and assigned (on the signing-account scope):

- `Artifact Signing Identity Verifier` — needed to create the identity validation.
- `Artifact Signing Certificate Profile Signer` — needed to actually sign.

Both roles are assigned to the human Azure account **and** to the automation
service principal. Neither role is inherited from Owner/Contributor — they must be
granted explicitly.

> **Secrets are NOT in this repo.** Subscription ID, tenant ID, the service
> principal's app ID, and (especially) its client secret live only on the build
> machine at `~/.azure-signing/sp.env` (file mode `600`). Never commit that file
> or paste the secret anywhere. The SP secret expires ~1 year after creation
> (created 2026-06-24) — rotate before then.

---

## One-time setup status

1. [x] Provider `Microsoft.CodeSigning` registered.
2. [x] Signing account + resource group created.
3. [x] Both signing roles assigned (human account + automation SP).
4. [ ] **Identity validation** — Individual → Public, completed in the Azure
   portal. *Manual, cannot be scripted; requires a government photo ID via the
   Microsoft Authenticator app. One-time per identity, reusable across profiles.*
5. [ ] **Certificate profile** `z80cpmw-public` (Public Trust) created.

Steps 4 and 5 gate all signing. Step 4 is a human portal action; step 5 is a one
-liner once step 4 shows **Completed** (see below).

### Step C — create the certificate profile (after identity validation Completed)

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
beta installers that leave your machine, use `PublicTrust`.

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
service principal. For the SP:

```bash
source ~/.azure-signing/sp.env        # sets AZURE_CLIENT_ID / TENANT_ID / CLIENT_SECRET / SUBSCRIPTION_ID
az login --service-principal -u "$AZURE_CLIENT_ID" -p "$AZURE_CLIENT_SECRET" -t "$AZURE_TENANT_ID"
```

The Microsoft signtool/dotnet-sign tooling also reads those same `AZURE_*` env
vars directly via DefaultAzureCredential — no explicit `az login` needed if the
env file is sourced.

### Option 1 — Windows: `signtool` + Trusted Signing dlib  *(integrates with the existing build scripts)*

This is the most native path for this project because the build already runs on
Windows (MSBuild + the PowerShell packaging scripts), and `build-msix.ps1` already
shells out to `signtool`.

1. Install the dlib (one-time). Either the NuGet package
   `Microsoft.Trusted.Signing.Client` (gives `bin\x64\Azure.CodeSigning.Dlib.dll`)
   or the PowerShell module:

   ```powershell
   Install-Module -Name TrustedSigning -Scope CurrentUser
   ```

2. Create a metadata file `trusted-signing.json` next to the build:

   ```json
   {
     "Endpoint": "https://eus.codesigning.azure.net/",
     "CodeSigningAccountName": "ms-code-sign-account",
     "CertificateProfileName": "z80cpmw-public"
   }
   ```

3. Sign (run once for the inner exe, once for the finished installer):

   ```bat
   signtool.exe sign /v /fd SHA256 ^
     /tr "http://timestamp.acs.microsoft.com" /td SHA256 ^
     /dlib "<path>\Azure.CodeSigning.Dlib.dll" ^
     /dmdf "<path>\trusted-signing.json" ^
     "dist\z80cpmw-1.0.17-setup.exe"
   ```

Where to hook it into the existing scripts:

- `packaging/scripts/build-nsis.ps1`: sign `bin\Release\z80cpmw.exe` right after
  the MSBuild step (Step 1), and sign the moved `dist\...-setup.exe` at the end
  (after Step 5).
- `packaging/scripts/build-msix.ps1`: the `-Beta` switch already wires this in — it
  rewrites the manifest Publisher to the cert subject, packs, then calls the signing
  kit's `sign.ps1` (signtool + dlib) and verifies. The kit folder defaults to
  `$env:Z80CPMW_SIGNING_KIT` (else `C:\temp\in\z80cpmw-signing-kit`); override with
  `-SigningKit <dir>`. The legacy `-CertificatePath` (`.pfx`) path is retained only
  for local self-signed testing.

### Option 2 — `dotnet sign` (Windows convenience wrapper)

```powershell
dotnet tool install --global sign --prerelease
sign code trusted-signing "dist\z80cpmw-1.0.17-setup.exe" `
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
runs anywhere, so it can sign the NSIS installer and the app exe from the Linux
build box or a Linux CI runner. (It does **not** sign MSIX.)

```bash
# Install (Ubuntu): download the .deb from https://github.com/ebourg/jsign/releases
#   sudo apt install ./jsign_<ver>_all.deb        # needs a Java 8+ runtime
# or run the standalone jar: java -jar jsign-<ver>.jar ...

source ~/.azure-signing/sp.env
az login --service-principal -u "$AZURE_CLIENT_ID" -p "$AZURE_CLIENT_SECRET" -t "$AZURE_TENANT_ID"
TOKEN=$(az account get-access-token --resource https://codesigning.azure.net --query accessToken -o tsv)

jsign \
  --storetype TRUSTEDSIGNING \
  --keystore  https://eus.codesigning.azure.net \
  --storepass "$TOKEN" \
  --alias     "ms-code-sign-account/z80cpmw-public" \
  z80cpmw-1.0.17-setup.exe
```

> **No trailing slash on `--keystore`.** jsign concatenates the path onto this
> URL, so `https://eus.codesigning.azure.net/` produces a `//codesigningaccounts`
> double slash and the sign call 404s. Use `https://eus.codesigning.azure.net`.

`jsign` timestamps automatically with this storetype. The access token is
short-lived (~1 hour), so fetch it right before signing.

This exact command has been verified end-to-end from Linux: it signs a real
z80cpmw PE with the Public Trust cert (subject `CN=Aaron Wohl, …`) and an embedded
Microsoft RFC-3161 timestamp.

---

## Verify a signature

- Windows: `signtool verify /pa /v "dist\z80cpmw-1.0.17-setup.exe"`, or right-click
  the file → **Properties → Digital Signatures**.
- Cross-platform: `osslsigncode verify z80cpmw-1.0.17-setup.exe`.

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

- Never commit `~/.azure-signing/` or any client secret / access token.
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
