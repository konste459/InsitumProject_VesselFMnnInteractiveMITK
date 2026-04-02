#pragma once

#include <QmitkSegWithPreviewToolGUIBase.h>
#include <MitkVesselFMUIExports.h>

class QLineEdit;
class QDoubleSpinBox;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QLabel;

namespace mitk { class VesselFMSegTool3D; }

class MITKVESSELFMUI_EXPORT QmitkVesselFMSegTool3DGUI : public QmitkSegWithPreviewToolGUIBase
{
  Q_OBJECT

public:
  mitkClassMacro(QmitkVesselFMSegTool3DGUI, QmitkSegWithPreviewToolGUIBase);
  itkFactorylessNewMacro(Self);
  itkCloneMacro(Self);

protected:
  // IMPORTANT: base has no default ctor; must call QmitkSegWithPreviewToolGUIBase(bool,...)
  QmitkVesselFMSegTool3DGUI();
  ~QmitkVesselFMSegTool3DGUI() override = default;

  void InitializeUI(QBoxLayout* mainLayout) override;
  void ConnectNewTool(mitk::SegWithPreviewTool* tool) override;
  void DisconnectOldTool(mitk::SegWithPreviewTool* tool) override;

private slots:
  void OnSettingsChanged();
  void OnRunPreviewClicked();
  void OnGetVolumeClicked();


private:
  mitk::VesselFMSegTool3D* m_Tool = nullptr;

  QLineEdit* m_Device = nullptr;
  QLineEdit* m_Ckpt = nullptr;
  QLineEdit* m_FileApp = nullptr;

  QDoubleSpinBox* m_Thr = nullptr;
  QCheckBox* m_UseRoi = nullptr;
  QSpinBox* m_PadVox = nullptr;

  QPushButton* m_RunPreviewBtn = nullptr;

  QLabel* m_VolumeLabel = nullptr;
  QLabel* m_MetricsLabel = nullptr;
  QPushButton* m_GetVolumeBtn = nullptr;
};



  