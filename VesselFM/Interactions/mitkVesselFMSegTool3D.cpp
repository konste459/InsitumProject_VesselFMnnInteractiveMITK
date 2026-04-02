#include "mitkVesselFMSegTool3D.h"
#include <mitkToolFactoryMacro.h>

#include <mitkIOUtil.h>
#include <mitkImage.h>
#include <mitkProcessExecutor.h>
#include <mitkExceptionMacro.h>
#include <mitkLog.h>
#include <mitkUIDGenerator.h>
#include <mitkImageCast.h>

#include <usModuleResource.h>

#include <itkImage.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>

#include <QCoreApplication>

#include <filesystem>
#include <limits>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace
{
  using LabelPixel = mitk::Label::PixelType;
  using LabelItk   = itk::Image<LabelPixel, 3>;
  using MaskItk    = itk::Image<unsigned char, 3>;

  itk::ImageRegion<3> ComputeRoiFromLabelImage(const LabelItk* seg,
                                               LabelPixel labelValue,
                                               unsigned int padVoxels)
  {
    const auto full = seg->GetLargestPossibleRegion();
    using IdxT = itk::Index<3>::IndexValueType;

    itk::Index<3> minIdx;
    itk::Index<3> maxIdx;
    minIdx.Fill(std::numeric_limits<IdxT>::max());
    maxIdx.Fill(std::numeric_limits<IdxT>::lowest());

    bool found = false;

    itk::ImageRegionConstIterator<LabelItk> it(seg, full);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
      if (it.Get() == labelValue)
      {
        const auto idx = it.GetIndex();
        for (int d = 0; d < 3; ++d)
        {
          minIdx[d] = std::min<IdxT>(minIdx[d], idx[d]);
          maxIdx[d] = std::max<IdxT>(maxIdx[d], idx[d]);
        }
        found = true;
      }
    }

    if (!found)
    {
      return full; // fallback to whole image
    }

    itk::Index<3> start;
    itk::Size<3> size;

    const auto fullStart = full.GetIndex();
    const auto fullSize  = full.GetSize();

    for (int d = 0; d < 3; ++d)
    {
      const IdxT fullMin = fullStart[d];
      const IdxT fullMax = fullStart[d] + static_cast<IdxT>(fullSize[d]) - static_cast<IdxT>(1);

      const IdxT s = std::max<IdxT>(fullMin, minIdx[d] - static_cast<IdxT>(padVoxels));
      const IdxT e = std::min<IdxT>(fullMax, maxIdx[d] + static_cast<IdxT>(padVoxels));

      start[d] = s;
      size[d]  = static_cast<itk::Size<3>::SizeValueType>(e - s + static_cast<IdxT>(1));
    }

    return itk::ImageRegion<3>(start, size);
  }

  std::string ReadTextFile(const fs::path& p)
  {
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in)
      return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  std::string TailString(const std::string& s, std::size_t maxChars)
  {
    if (s.size() <= maxChars)
      return s;
    return s.substr(s.size() - maxChars);
  }

  std::string TrimCRLF(std::string s)
  {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
      s.pop_back();
    while (!s.empty() && (s.front() == '\r' || s.front() == '\n' || s.front() == ' ' || s.front() == '\t'))
      s.erase(s.begin());
    return s;
  }

  std::string QuoteForCmd(const std::string& s)
  {
    // Basic quoting for cmd.exe. Good enough for paths with spaces.
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s)
    {
      if (c == '"')
        out += "\\\"";
      else
        out.push_back(c);
    }
    out.push_back('"');
    return out;
  }

  fs::path MakeUniqueTempDir()
  {
    mitk::UIDGenerator uid("vesselfm_");
    fs::path dir = fs::temp_directory_path() / uid.GetUID();
    fs::create_directories(dir);
    return dir;
  }

  
 fs::path GetManagedPythonExecutable()
  {
    const fs::path appDir = fs::path(QCoreApplication::applicationDirPath().toStdString());

  #ifdef _WIN32
    const char* pythonRel = "python/vesselfm_env/Scripts/python.exe";
  #else
    const char* pythonRel = "python/vesselfm_env/bin/python";
  #endif

    const std::vector<fs::path> candidates = {
      (appDir / pythonRel).lexically_normal(),
      (appDir / ".." / pythonRel).lexically_normal(),
      (appDir / ".." / ".." / pythonRel).lexically_normal(),
      (appDir / ".." / ".." / ".." / pythonRel).lexically_normal()
    };

    for (const auto& c : candidates)
    {
      MITK_INFO << "VesselFMSegTool3D: probing Python candidate: " << c.string();
      if (fs::exists(c))
        return c;
    }

    return candidates.back();
  }

  std::string ToGenericString(const fs::path& p)
  {
    return p.lexically_normal().generic_string();
  }

  std::string QuoteHydraValue(const std::string& value)
  {
    return "\"" + value + "\"";
  }

  bool LooksLikePlaceholderCheckpoint(const std::string& path)
  {
    const auto trimmed = TrimCRLF(path);
    return trimmed.empty() || trimmed == "/path/to/ckpt.pt" || trimmed == " /path/to/ckpt.pt";
  }
}

namespace mitk
{
  VesselFMSegTool3D::VesselFMSegTool3D()
    : SegWithPreviewTool(true) // lazyDynamicPreviews=true -> show Preview button and run inference on demand
  {
    // Preview tools need consistent transfer settings, otherwise Confirm can fail
    this->SetMergeStyle(MultiLabelSegmentation::MergeStyle::Merge);
    this->SetOverwriteStyle(MultiLabelSegmentation::OverwriteStyle::RegardLocks);
    this->SetResetsToEmptyPreview(true);

    // IMPORTANT: By default SegWithPreviewTool expects a selected label for mapping when scope is ActiveLabel.
    this->SetLabelTransferScope(LabelTransferScope::ActiveLabel);
    this->SetLabelTransferMode(LabelTransferMode::MapLabel);

    // Keep the tool active after confirming while debugging (prevents "it quits" confusion)
    this->KeepActiveAfterAcceptOn();

    // Optional: make preview easier to spot
    this->UseSpecialPreviewColorOn();
  }

  const char* VesselFMSegTool3D::GetName() const
  {
    return "VesselFM (3D)";
  }

  const char** VesselFMSegTool3D::GetXPM() const
  {
    static const char* VesselFM3D_xpm[] =
    {
      "16 16 2 1",
      "  c None",
      ". c #2E86DE",
      "................",
      "................",
      ".... ..    .. ...",
      "...  ..    ..  ..",
      "..   ..    ..   .",
      "..   ..    ..   .",
      "..   ..    ..   .",
      "..   ..    ..   .",
      "..    ..  ..    .",
      "...    ....    ..",
      "....    ..    ...",
      ".....        .....",
      "................",
      "................",
      "................",
      "................"
    };
    return VesselFM3D_xpm;
  }

  us::ModuleResource VesselFMSegTool3D::GetIconResource() const
  {
    return us::ModuleResource(); 
  }

  void VesselFMSegTool3D::Activated()
  {
    Superclass::Activated();

    // Make sure the transfer configuration is what we expect
    this->SetLabelTransferScope(LabelTransferScope::ActiveLabel);
    this->SetLabelTransferMode(LabelTransferMode::MapLabel);
    this->SetMergeStyle(MultiLabelSegmentation::MergeStyle::Merge);
    this->SetOverwriteStyle(MultiLabelSegmentation::OverwriteStyle::RegardLocks);

    // Ensure that at least one label is selected (required by the base tool for transfer mapping)
    const auto activeLabelValue = this->GetUserDefinedActiveLabel();
    if (activeLabelValue == 0)
    {
      MITK_WARN << "VesselFMSegTool3D: No active label selected. Create/select a label before preview/confirm.";
      this->GeneralMessage.Send("VesselFM: Please create/select a label before running Preview/Confirm.");
    }
    else
    {
      this->SetSelectedLabels({activeLabelValue});
      this->GeneralMessage.Send("VesselFM: Click Preview to run inference, then Confirm segmentation to apply.");
    }
  }

  void VesselFMSegTool3D::UpdatePrepare()
  {
    Superclass::UpdatePrepare();

    const auto activeLabelValue = this->GetUserDefinedActiveLabel();
    if (activeLabelValue == 0)
    {
      this->ErrorMessage.Send("VesselFM: No active label selected. Create/select a label first.");
      mitkThrow() << "VesselFMSegTool3D: No active label selected (label value 0).";
    }

    // Ensure that the base class sees a selected label for transfer/mapping.
    this->SetSelectedLabels({activeLabelValue});
  }

  void VesselFMSegTool3D::ConfirmCleanUp()
  {
    MITK_INFO << "VesselFMSegTool3D: ConfirmCleanUp()";
    Superclass::ConfirmCleanUp();
  }

  void VesselFMSegTool3D::DoUpdatePreview(const Image* inputAtTimeStep,
                                          const Image* oldSegAtTimeStep,
                                          MultiLabelSegmentation* previewImage,
                                          TimeStepType /*timeStep*/)
  {
    if (nullptr == inputAtTimeStep || nullptr == previewImage)
    {
      mitkThrow() << "VesselFMSegTool3D: invalid input/preview pointers.";
    }

    const fs::path pythonExe = GetManagedPythonExecutable();
    if (!fs::exists(pythonExe))
    {
      mitkThrow() << "VesselFMSegTool3D: managed Python environment not found: "
                  << pythonExe.string();
    }

    const auto activeLabelValue = static_cast<mitk::Label::PixelType>(this->GetActiveLabelValueOfPreview());
    if (activeLabelValue == mitk::Label::UNLABELED_VALUE)
    {
      mitkThrow() << "VesselFMSegTool3D: No active label selected. "
                     "Please create/select a label in the Segmentation view before confirming.";
    }

    this->SetSelectedLabels({activeLabelValue});

    fs::path workDir = MakeUniqueTempDir();
    MITK_INFO << "VesselFMSegTool3D: workDir=" << workDir.string();

    const fs::path imageDir = workDir / "images";
    fs::create_directories(imageDir);

    const std::string imageFileEnding = "nii.gz";
    const std::string fileExt = ".nii.gz";

    // Keep the input filename stable. This makes the expected prediction path deterministic.
    const std::string inputFileName = "input" + fileExt;
    const fs::path inputImagePath = imageDir / inputFileName;

    mitk::IOUtil::Save(inputAtTimeStep, inputImagePath.string());

    std::ostringstream thr;
    thr.setf(std::ios::fixed);
    thr << std::setprecision(6) << m_MergingThreshold;

    const std::string imageDirHydra = ToGenericString(imageDir);
    const std::string outDirHydra   = ToGenericString(workDir);
    const std::string hydraRunDir   = ToGenericString(workDir / "hydra_run");

    const fs::path stdoutPath = workDir / "stdout.txt";
    const fs::path stderrPath = workDir / "stderr.txt";
    const fs::path cmdPath    = workDir / "run_vesselfm.cmd";

    {
      std::ofstream cmd(cmdPath);
      if (!cmd)
      {
        mitkThrow() << "VesselFMSegTool3D: Cannot write debug cmd: " << cmdPath.string();
      }
      // Match old runner behavior: run in .../seg so hydra config_path="configs" resolves.

      // We keep a .cmd wrapper on Windows because it makes Hydra debugging much easier
      // and preserves stdout/stderr redirection into files.
      cmd << "@echo off\n";
      cmd << "setlocal\n";
      cmd << "set HYDRA_FULL_ERROR=1\n";
      cmd << "set PYTHONUNBUFFERED=1\n";

      cmd << QuoteForCmd(TrimCRLF(pythonExe.string())) << " -u -m vesselfm.seg.inference ^\n";
      cmd << "  " << QuoteForCmd("image_path=" + QuoteHydraValue(imageDirHydra)) << " ^\n";
      cmd << "  " << QuoteForCmd("output_folder=" + QuoteHydraValue(outDirHydra)) << " ^\n";
      cmd << "  " << QuoteForCmd("image_file_ending=" + imageFileEnding) << " ^\n";
      cmd << "  " << QuoteForCmd("device=" + QuoteHydraValue(TrimCRLF(m_Device))) << " ^\n";

      if (!TrimCRLF(m_FileApp).empty())
      {
        cmd << "  " << QuoteForCmd("file_app=" + QuoteHydraValue(TrimCRLF(m_FileApp))) << " ^\n";
      }
      else
      {
        cmd << "  " << QuoteForCmd("file_app=\"\"") << " ^\n";
      }

      cmd << "  " << QuoteForCmd("merging.threshold=" + thr.str()) << " ^\n";

      if (!LooksLikePlaceholderCheckpoint(m_CheckpointPath))
      {
        const fs::path ckptPath = fs::path(TrimCRLF(m_CheckpointPath));
        if (!fs::exists(ckptPath))
        {
          mitkThrow() << "VesselFMSegTool3D: checkpoint does not exist: " << ckptPath.string();
        }

        cmd << "  " << QuoteForCmd("ckpt_path=" + QuoteHydraValue(ToGenericString(ckptPath))) << " ^\n";
      }

      cmd << "  " << QuoteForCmd("hydra.run.dir=" + QuoteHydraValue(hydraRunDir)) << " ^\n";
      cmd << "  " << QuoteForCmd("hydra.output_subdir=null") << " "
          << "1> " << QuoteForCmd(stdoutPath.string())
          << " 2> " << QuoteForCmd(stderrPath.string()) << "\n";

      cmd << "set \"RC=%ERRORLEVEL%\"\n";
      cmd << "endlocal & exit /b %RC%\n";
    }

    // Execute the cmd file (it writes stdout/stderr into workDir)
    mitk::ProcessExecutor::ArgumentListType execArgs;
    execArgs.push_back("/C");
    execArgs.push_back(cmdPath.string());

    auto executor = mitk::ProcessExecutor::New();
    const int exitCode = executor->Execute(workDir.string(), "cmd.exe", execArgs);

    // Read captured output (note: tqdm/progress bars often write to stderr even on success)
    const std::string stdOut = ReadTextFile(stdoutPath);
    const std::string stdErr = ReadTextFile(stderrPath);

    // This assumes the Python side writes a stable file name.
    // If vesselFM currently produces a different name due to file_app handling,
    // either make the Python side always emit input_pred.nii.gz for MITK usage,
    // or update this contract here.
    const fs::path predPath = workDir / "input_pred.nii.gz";

    if (exitCode != 0)
    {
      if (fs::exists(predPath))
      {
        MITK_WARN << "VesselFMSegTool3D: inference returned non-zero exitCode=" << exitCode
                  << " but output exists. Continuing.";
        MITK_WARN << "VesselFMSegTool3D: stderr (tail):\n" << TailString(stdErr, 2000);
        MITK_WARN << "VesselFMSegTool3D: stdout (tail):\n" << TailString(stdOut, 2000);
      }
      else
      {
        mitkThrow() << "VesselFMSegTool3D: inference failed with exit code " << exitCode
                    << "\n--- stderr (tail) ---\n" << TailString(stdErr, 8000)
                    << "\n--- stdout (tail) ---\n" << TailString(stdOut, 8000)
                    << "\n--- rerun ---\n" << cmdPath.string()
                    << "\n--- workDir ---\n" << workDir.string();
      }
    }

    MITK_INFO << "VesselFMSegTool3D: expecting output=" << predPath.string();

    if (!fs::exists(predPath))
    {
      mitkThrow() << "VesselFMSegTool3D: expected output not found: " << predPath.string();
    }

    auto loaded = IOUtil::Load(predPath.string());
    if (loaded.empty())
    {
      mitkThrow() << "VesselFMSegTool3D: failed to load output: " << predPath.string();
    }

    auto predMitk = dynamic_cast<Image*>(loaded.front().GetPointer());
    if (!predMitk)
    {
      mitkThrow() << "VesselFMSegTool3D: output is not an image: " << predPath.string();
    }

    predMitk->SetGeometry(inputAtTimeStep->GetGeometry()->Clone());

    itk::ImageRegion<3> roi;
    if (m_UseRoiFromExistingSegmentation && oldSegAtTimeStep != nullptr)
    {
      LabelItk::Pointer oldSegItk;
      CastToItkImage(oldSegAtTimeStep, oldSegItk);
      roi = ComputeRoiFromLabelImage(oldSegItk.GetPointer(),
                                     static_cast<LabelPixel>(activeLabelValue),
                                     m_RoiPaddingVoxels);
    }
    else
    {
      MaskItk::Pointer predItk;
      CastToItkImage(predMitk, predItk);
      roi = predItk->GetLargestPossibleRegion();
    }

    const auto groupID = previewImage->GetGroupIndexOfLabel(activeLabelValue);
    Image* groupImage  = previewImage->GetGroupImage(groupID);

    LabelItk::Pointer groupItk;
    CastToItkImage(groupImage, groupItk);

    MaskItk::Pointer predItk;
    CastToItkImage(predMitk, predItk);

    itk::ImageRegionIterator<LabelItk> outIt(groupItk, roi);
    itk::ImageRegionConstIterator<MaskItk> inIt(predItk, roi);

    std::size_t voxelsSet = 0;
    for (outIt.GoToBegin(), inIt.GoToBegin(); !outIt.IsAtEnd(); ++outIt, ++inIt)
    {
      const auto pred = inIt.Get();
      const auto cur  = outIt.Get();

      if (pred > 0)
      {
        if (cur != activeLabelValue)
        {
          outIt.Set(activeLabelValue);
        }
        ++voxelsSet;
      }
      else
      {
        // If the preview was not empty (or got re-used), remove previous active-label voxels
        if (cur == activeLabelValue)
        {
          outIt.Set(0);
        }
      }
    }

    if (voxelsSet == 0)
    {
      MITK_WARN << "VesselFMSegTool3D: Prediction produced an empty mask. Nothing will be applied on Confirm.";
      this->GeneralMessage.Send("VesselFM: Prediction is empty (no voxels). Check model/output path or threshold.");
    }
    else
    {
      MITK_INFO << "VesselFMSegTool3D: Preview updated. Voxels set for label "
                << activeLabelValue << ": " << voxelsSet;
    }

    groupImage->Modified();
    previewImage->Modified();
  }


  double VesselFMSegTool3D::ComputeActiveLabelVolumeMm3() const
  {
    return this->ComputeActiveLabelMetrics().volumeMm3;
  }

  VesselFMSegTool3D::LabelMetrics VesselFMSegTool3D::ComputeActiveLabelMetrics() const
  {
    LabelMetrics metrics;

    const auto* targetSeg = this->GetTargetSegmentation();
    if (!targetSeg)
    {
      mitkThrow() << "No target segmentation available.";
    }

    const auto activeLabelValue =
      static_cast<mitk::Label::PixelType>(this->GetUserDefinedActiveLabel());

    if (activeLabelValue == mitk::Label::UNLABELED_VALUE)
    {
      mitkThrow() << "No active label selected.";
    }

    const auto groupID = targetSeg->GetGroupIndexOfLabel(activeLabelValue);
    const mitk::Image* groupImage = targetSeg->GetGroupImage(groupID);
    if (!groupImage)
    {
      mitkThrow() << "Could not access group image for active label.";
    }

    LabelItk::Pointer segItk;
    CastToItkImage(groupImage, segItk);

    const auto region = segItk->GetLargestPossibleRegion();
    const auto spacing = groupImage->GetGeometry()->GetSpacing();

    bool found = false;
    itk::Index<3> minIdx;
    itk::Index<3> maxIdx;

    itk::ImageRegionConstIterator<LabelItk> it(segItk, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
      if (it.Get() != activeLabelValue)
        continue;

      const auto idx = it.GetIndex();

      if (!found)
      {
        minIdx = idx;
        maxIdx = idx;
        found = true;
      }
      else
      {
        for (unsigned int d = 0; d < 3; ++d)
        {
          if (idx[d] < minIdx[d]) minIdx[d] = idx[d];
          if (idx[d] > maxIdx[d]) maxIdx[d] = idx[d];
        }
      }

      ++metrics.voxelCount;
    }

    if (!found)
    {
      return metrics; // returns zeros
    }

    const double voxelVolumeMm3 =
      static_cast<double>(spacing[0]) *
      static_cast<double>(spacing[1]) *
      static_cast<double>(spacing[2]);

    metrics.volumeMm3 = static_cast<double>(metrics.voxelCount) * voxelVolumeMm3;
    metrics.volumeCc  = metrics.volumeMm3 / 1000.0;

    metrics.widthMm  = static_cast<double>(maxIdx[0] - minIdx[0] + 1) * spacing[0];
    metrics.heightMm = static_cast<double>(maxIdx[1] - minIdx[1] + 1) * spacing[1];
    metrics.depthMm  = static_cast<double>(maxIdx[2] - minIdx[2] + 1) * spacing[2];

    return metrics;
  }
  MITK_TOOL_MACRO(MITKVESSELFM_EXPORT, VesselFMSegTool3D, "VesselFM 3D Segmentation Tool");
}