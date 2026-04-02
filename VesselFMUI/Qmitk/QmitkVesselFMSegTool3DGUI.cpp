#include "QmitkVesselFMSegTool3DGUI.h"
#include <mitkToolFactoryMacro.h>

#include <mitkVesselFMSegTool3D.h>

#include <QWidget>
#include <QBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

MITK_TOOL_GUI_MACRO(MITKVESSELFMUI_EXPORT, QmitkVesselFMSegTool3DGUI, "GUI for VesselFM 3D Tool");

QmitkVesselFMSegTool3DGUI::QmitkVesselFMSegTool3DGUI()
  : QmitkSegWithPreviewToolGUIBase(false)
{
}

void QmitkVesselFMSegTool3DGUI::InitializeUI(QBoxLayout* mainLayout)
{
  Superclass::InitializeUI(mainLayout);
  

  auto* wrapper = new QWidget(this);
  auto* form = new QFormLayout(wrapper);

  m_Device  = new QLineEdit(wrapper);
  m_Ckpt    = new QLineEdit(wrapper);
  m_FileApp = new QLineEdit(wrapper);

  m_Device->setText("cuda:0");

  m_Thr = new QDoubleSpinBox(wrapper);
  m_Thr->setRange(0.0, 1.0);
  m_Thr->setDecimals(3);
  m_Thr->setSingleStep(0.01);
  m_Thr->setValue(0.5);

  m_UseRoi = new QCheckBox("Restrict to ROI around existing segmentation", wrapper);
  m_UseRoi->setChecked(true);

  m_PadVox = new QSpinBox(wrapper);
  m_PadVox->setRange(0, 200);
  m_PadVox->setValue(3);

  m_GetVolumeBtn = new QPushButton("Get Label Metrics", wrapper);
  m_VolumeLabel = new QLabel("Volume: not computed yet", wrapper);
  m_MetricsLabel = new QLabel("Dimensions: not computed yet", wrapper);

  m_VolumeLabel->setWordWrap(true);
  m_MetricsLabel->setWordWrap(true);

  form->addRow("Device", m_Device);
  form->addRow("Checkpoint path (optional)", m_Ckpt);
  form->addRow("Merging threshold", m_Thr);
  form->addRow("file_app", m_FileApp);
  form->addRow(m_UseRoi);
  form->addRow("ROI padding (voxels)", m_PadVox);
  form->addRow(m_GetVolumeBtn);
  form->addRow("Active label volume", m_VolumeLabel);
  form->addRow("Active label dimensions", m_MetricsLabel);


  m_RunPreviewBtn = new QPushButton("Run Preview", wrapper);
  m_RunPreviewBtn->setToolTip("Runs VesselFM inference and writes the result into the preview segmentation.\n"
                              "After that, use 'Confirm segmentation' to apply it.");
  form->addRow(m_RunPreviewBtn);

  mainLayout->insertWidget(0, wrapper);

  connect(m_Device,  &QLineEdit::editingFinished, this, &QmitkVesselFMSegTool3DGUI::OnSettingsChanged);
  connect(m_Ckpt,    &QLineEdit::editingFinished, this, &QmitkVesselFMSegTool3DGUI::OnSettingsChanged);
  connect(m_FileApp, &QLineEdit::editingFinished, this, &QmitkVesselFMSegTool3DGUI::OnSettingsChanged);

  connect(m_Thr,    qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QmitkVesselFMSegTool3DGUI::OnSettingsChanged);
  connect(m_UseRoi, &QCheckBox::toggled,                              this, &QmitkVesselFMSegTool3DGUI::OnSettingsChanged);
  connect(m_PadVox, qOverload<int>(&QSpinBox::valueChanged),          this, &QmitkVesselFMSegTool3DGUI::OnSettingsChanged);

  connect(m_GetVolumeBtn, &QPushButton::clicked, this, &QmitkVesselFMSegTool3DGUI::OnGetVolumeClicked);
  connect(m_RunPreviewBtn, &QPushButton::clicked, this, &QmitkVesselFMSegTool3DGUI::OnRunPreviewClicked);
}

void QmitkVesselFMSegTool3DGUI::ConnectNewTool(mitk::SegWithPreviewTool* tool)
{
  Superclass::ConnectNewTool(tool);
  m_Tool = dynamic_cast<mitk::VesselFMSegTool3D*>(tool);

  if (m_RunPreviewBtn)
    m_RunPreviewBtn->setEnabled(m_Tool != nullptr);

  OnSettingsChanged();
}

void QmitkVesselFMSegTool3DGUI::DisconnectOldTool(mitk::SegWithPreviewTool* tool)
{
  m_Tool = nullptr;
  if (m_RunPreviewBtn)
    m_RunPreviewBtn->setEnabled(false);

  Superclass::DisconnectOldTool(tool);
}

void QmitkVesselFMSegTool3DGUI::OnSettingsChanged()
{
  if (!m_Tool) return;

  m_Tool->SetDevice(m_Device->text().toStdString());
  m_Tool->SetCheckpointPath(m_Ckpt->text().toStdString());
  m_Tool->SetMergingThreshold(m_Thr->value());
  m_Tool->SetFileApp(m_FileApp->text().toStdString());
  m_Tool->SetUseRoiFromExistingSegmentation(m_UseRoi->isChecked());
  m_Tool->SetRoiPaddingVoxels(static_cast<unsigned int>(m_PadVox->value()));
}

void QmitkVesselFMSegTool3DGUI::OnGetVolumeClicked()
{
  if (!m_Tool || !m_VolumeLabel || !m_MetricsLabel)
    return;

  try
  {
    const auto metrics = m_Tool->ComputeActiveLabelMetrics();

    m_VolumeLabel->setText(
      QString("Label volume: %1 cc (%2 mm³), voxels: %3")
        .arg(metrics.volumeCc, 0, 'f', 3)
        .arg(metrics.volumeMm3, 0, 'f', 2)
        .arg(QString::number(metrics.voxelCount)));

    m_MetricsLabel->setText(
      QString("W: %1 mm, H: %2 mm, D: %3 mm")
        .arg(metrics.widthMm, 0, 'f', 2)
        .arg(metrics.heightMm, 0, 'f', 2)
        .arg(metrics.depthMm, 0, 'f', 2));
  }
  catch (const mitk::Exception& e)
  {
    m_VolumeLabel->setText(QString("Metrics failed: %1").arg(e.GetDescription()));
    m_MetricsLabel->setText("Dimensions: unavailable");
  }
}

void QmitkVesselFMSegTool3DGUI::OnRunPreviewClicked()
{
  if (!m_Tool)
    return;

  OnSettingsChanged();

  try
  {
    MITK_INFO << "QmitkVesselFMSegTool3DGUI: Run Preview clicked";

    // Force preview computation. If the tool was created without lazy previews, this still works.
    // If it was created WITH lazy previews, this is exactly the intended way to run the heavy algorithm.
    m_Tool->UpdatePreview(/*ignoreLazyPreviewSetting=*/true);
  }
  catch (const mitk::Exception& e)
  {
    MITK_ERROR << "VesselFM: UpdatePreview failed: " << e.GetDescription();
  }
  catch (...)
  {
    MITK_ERROR << "VesselFM: UpdatePreview failed with an unknown exception.";
  }
}
