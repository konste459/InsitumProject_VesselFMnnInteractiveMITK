project(MitkVesselFMUI)

include(files.cmake)

mitk_create_module(
  DEPENDS
    MitkVesselFM
    MitkQtWidgets
    MitkQtWidgetsExt
    MitkSegmentationUI
  PACKAGE_DEPENDS
    Qt6|Widgets+Gui

  AUTOLOAD_WITH
    MitkSegmentationUI

)
