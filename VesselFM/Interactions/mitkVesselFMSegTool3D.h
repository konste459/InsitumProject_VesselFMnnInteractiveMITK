#pragma once

#include <mitkSegWithPreviewTool.h>
#include <mitkLabelSetImage.h>   

#include <MitkVesselFMExports.h>
#include <cstddef>
#include <string>

namespace us
{
  class ModuleResource;
}

namespace mitk
{
  class MultiLabelSegmentation;

  class MITKVESSELFM_EXPORT VesselFMSegTool3D : public SegWithPreviewTool
  {
  public:
    mitkClassMacro(VesselFMSegTool3D, SegWithPreviewTool);
    itkFactorylessNewMacro(Self);
    itkCloneMacro(Self);

    const char* GetName() const override;
    const char** GetXPM() const override;
    us::ModuleResource GetIconResource() const override;

    struct LabelMetrics
    {
      std::size_t voxelCount = 0;
      double volumeMm3 = 0.0;
      double volumeCc = 0.0;

      double widthMm = 0.0;
      double heightMm = 0.0;
      double depthMm = 0.0;
    };

    double ComputeActiveLabelVolumeMm3() const;
    LabelMetrics ComputeActiveLabelMetrics() const;

    void Activated() override;

    void SetDevice(const std::string& d) { m_Device = d; }
    const std::string& GetDevice() const { return m_Device; }

    void SetCheckpointPath(const std::string& p) { m_CheckpointPath = p; }
    const std::string& GetCheckpointPath() const { return m_CheckpointPath; }

    void SetMergingThreshold(double t) { m_MergingThreshold = t; }
    double GetMergingThreshold() const { return m_MergingThreshold; }

    void SetFileApp(const std::string& fa) { m_FileApp = fa; }
    const std::string& GetFileApp() const { return m_FileApp; }

    void SetUseRoiFromExistingSegmentation(bool v) { m_UseRoiFromExistingSegmentation = v; }
    bool GetUseRoiFromExistingSegmentation() const { return m_UseRoiFromExistingSegmentation; }

    void SetRoiPaddingVoxels(unsigned int v) { m_RoiPaddingVoxels = v; }
    unsigned int GetRoiPaddingVoxels() const { return m_RoiPaddingVoxels; }

  protected:
    VesselFMSegTool3D();
    ~VesselFMSegTool3D() override = default;

    void UpdatePrepare() override;
    void ConfirmCleanUp() override;

    void DoUpdatePreview(const Image* inputAtTimeStep,
                         const Image* oldSegAtTimeStep,
                         MultiLabelSegmentation* previewImage,
                         TimeStepType timeStep) override;

  private:
    std::string m_Device = "cuda:0";
    std::string m_CheckpointPath;
    std::string m_FileApp;
    double m_MergingThreshold = 0.5;

    bool m_UseRoiFromExistingSegmentation = true;
    unsigned int m_RoiPaddingVoxels = 3;
  };
}