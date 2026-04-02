# VesselFM MITK Plugin

Integration of **VesselFM** into **MITK Workbench** as a native plugin and segmentation tool.

This project enables running deep-learning-based vessel segmentation directly inside MITK, including:
- a **Segmentation Tool (Preview Tool)** for VesselFM inference and interactive workflows

---

# Features

- Run VesselFM directly inside MITK
- Export selected images automatically to NIfTI
- Execute Python-based inference via Hydra
- Load predicted segmentation masks back into MITK
- Interactive segmentation via Preview/Confirm workflow
- Configurable:
  - device (CPU / CUDA)
  - threshold for probabiltiy of counting into the segmentated pixel-> low (i.e. 0.2) more pixels will count into the final segmentation (could lead to more wrong segmented pixels but also maybe a more detailed vessel mask; medium (0.5) default option, balance; high (i.e. 0.8) less pixels are segmentated (potentially fewer bone missegmentation but also more sparse vessel mask)
  - restrict segmentation around existing roi with padding
  - compute volume and pixel spacing information
  - checkpoint path -> useful i.e. if you did pretraining
  - Get volume metrics -> after segmentation is done you can get additional volumetric information

---

# 📁 Repository Structure


Modules/VesselFM/
    Interactions/mitkVesselFMSegTool3D.cpp -> main logic file
    external/vesselFM -> added as a git submodule, it is the official repo for VesselFM. 
    (Before building, add __init__.py files to the vesselFM folder and subfolders so that it could be installed as a package correctly.)
    
Modules/VesselFMUI/
    Qmitk/QmitkVesselFMSegTool3DGUI.cpp -> main l0gic file
    (optional GUI files)

# ⚙️ Requirements
  - System: Windows 10/11
  - Visual Studio 2022 (MSVC)
  - CMake ≥ 3.20
  
# Dependencies
 - MITK source (e.g. 2025.08 / 2025.12)
 - Qt 6 (MSVC build!)
 - Python environment with:
 - torch
 - hydra-core
 - monai
 - VesselFM dependencies such as torch might be system and driver dependent 

1. MITK Superbuild

MITK must be built from source to ensure plugin compatibility
-> This is  a multi step process further information please check put the official build instructions from mitk: https://docs.mitk.org/nightly/BuildInstructionsPage.html

Example Directory layout:
C:\MITK-2025.12\                <- MITK source
C:\MITK_build\MITK-build\       <- build directory

2. After you built the MITK superbuild you mith have to built the workbench (again) if there is no .exe file yet
   -> run this command for this: cmake --build C:\MITK_build\MITK-build --config Release --target MitkWorkbench

3. If you make changes to the plugin rebuild only the change files i.e. with these commands
   - cmake --build C:\MITK_build\MITK-build --config Release --target MitkVesselFM
   - cmake --build C:\MITK_build\MITK-build --config Release --target MitkVesselFMUI

4. Run the plugin in MITK
   1. Load a file you want to segment
   2. Create a new segmentation label
   3. In the segmentation window choose 3D
   4. For the proposed nnInteractive_>vesselFM workflow: Choose nnInteractive
   5. Intialize and install dependencies(first time use only)
   6. Use the prompt tool to segment the aneurysm sac (i.e. via point prompt)
   7. Confirm segmentation
   8. Choose the vesselFM plugin
   9. Restrict mask around roi
   10. Choose threshold or leave at default as well as pixel number for the padding box around the roi from nnInteractive
   11. Preview segmentation
   12. Confirm segmentation
   13. Get volume metrics
