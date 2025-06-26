# Adding Metadata to DWF Sheets with AcDMMReactor in AutoCAD

Injecting metadata to layouts and entities while publishing to DWF

---

## ✅ Prerequisites

Before you begin, ensure you have the following installed:

- **AutoCAD 2026 SDK**
  
  - **Installed at:** `D:\ArxSDKs\arx2026` (Please adjust this path if your SDK is located elsewhere).
  - **Includes:**
    - `inc\` (headers and `.props` files)
    - `lib-x64\` (library binaries for x64)

- **Visual Studio 2022 or later**
  
  - Make sure the **"C++ Desktop Development"** workload is installed.

---

## 🛠️ Steps to Build

Follow these steps to compile your ARX plugin:

### 1. Open the Project in Visual Studio

Open the `.vcxproj` file (or the `.sln` file if you've created one) in Visual Studio. Ensure Visual Studio detects this as a **Dynamic Library (DLL)** project.

### 2. Verify and Customize SDK Path

In **Property Manager** (or directly in the `.vcxproj` file), confirm that the following `Import` entries point to your local SDK path:


```
<Import Project="D:\ArxSDKs\arx2026\inc\rxsdk_Debugcfg.props" />
<Import Project="D:\ArxSDKs\arx2026\inc\arx.props" />
```

⚠️ **Important:** Change `D:\ArxSDKs\arx2026` to your actual AutoCAD 2026 SDK installation path if it's different. These `.props` files automatically set necessary include/library paths and preprocessor macros for your project.

### 3. Source File Configuration

Ensure your project includes a source file (e.g., `main.cpp`) that contains:

- `acrxEntryPoint` function.
- Your reactor class definitions.
- All required includes (e.g., `aced.h`, `rxregsvc.h`).

Here's an example of the minimal top of your `main.cpp`:


```
#include "aced.h"
#include "rxregsvc.h"
#include "acpublishreactors.h"
#include "AcPublishUIReactors.h"
```

### 4. Set Build Configuration

In Visual Studio, select the following build options:

- **Platform:** `x64`
- **Configuration:** `Debug` or `Release`

### 5. Build

Hit **Ctrl+Shift+B** or navigate to `Build > Build Solution` in the Visual Studio menu.

---

## ✅ Output

Upon successful compilation, an `.arx` file (e.g., `main.arx`) will be generated in either the `x64\Debug\` or `x64\Release\` directory within your project folder.

---

## ✅ How to Test the ARX Plugin in AutoCAD 2026

### 🔧 Preconditions

Before testing, make sure:

- Your plugin has successfully compiled to an `.arx` file (e.g., `main.arx`).
- AutoCAD 2026 is installed and running.

### 🧪 Test Steps

#### Step 1: Load the ARX Plugin

1. Open AutoCAD 2026.

2. Run the command:
   
   ```
   APPLOAD
   ```

3. Browse to your built `.arx`  (e.g., `main.arx`).

4. Load it. If everything is built correctly, it should load silently or display a confirmation message.

#### Step 2: Prepare a Drawing

Open any `.dwg` file that contains entities and at least one layout with printable content. You might want to create a sample layout with blocks for metadata testing.

#### Step 3: Run the Publish Workflow

1. Run the command:
   
   ```
   PUBLISH
   ```

2. In the **Publish Dialog**:
   
   - Add your layout(s) if they are not already listed.
   - Set **Publish to:** `DWF`.
   - Click **Publish Options...**:
     - Choose whether to publish to a single or multi-sheet DWF. Your UI reactor should trigger at this point.
   - Click **Publish**.

### What to Expect:

- **Command Line Logging:** You should see messages logged in the AutoCAD command line from your `OnBeginSheet`, `OnBeginEntity`, and `OnEndEntity` callbacks.
- **OnEndSheet Behavior:**
  - A "Finished" message should be logged.
  - `help.html` should be attached as a page-level resource inside the DWF.
- **Metadata:** Metadata (like Layer, Handle, BlockName, etc.) will be added to the output DWF.
- **Reactor Registration:** `MyPublishReactor` ensures the metadata reactor is registered only during the actual publishing process.

---

## 📁 Inspect the DWF Output

Use **Autodesk Design Review** or any DWF viewer to inspect the generated DWF file. Verify:

- The presence and correctness of **Metadata tags**.
- The **Page resource** (`Help.html`) is properly attached.
- **Node-level associations** (e.g., entity handles).
