#pragma once

#include <tclap/CmdLine.h>

enum DataSet
{
    AZBA = 0,
    CELLS = 1,
    FIBER = 2,
    MOTTA2019SMALL = 3,
    PA66 = 4,
    WOLNY2020 = 5,
    XTMBATTERY = 6,
};

struct Config
{
    bool verbose = false;
    int render_width = 1902;
    int render_height = 1080;
    int render_frames = 300;
    bool offscreen = false;
    std::filesystem::path camera_import_file = {};
    std::filesystem::path camera_export_file = "./camera.cam";
    std::filesystem::path image_export_dir = "./";
    std::filesystem::path data_base_dir = "./";
    std::filesystem::path vcfg_base_dir = "./";
    std::filesystem::path csv_result_file = "./results.csv";
    DataSet data_set = AZBA;         ///< one of {AZBA, CELLS, FIBER, MOTTA2019SMALL, PA66, WOLNY2020, XTMBATTERY};
};


std::filesystem::path getDataInputPath(const Config& config, const DataSet data)
{
    std::filesystem::path postfix = {};
    switch (data)
    {
    // case ARA2016:
    //     postfix = "Ara2016/x0y0z0.hdf5";
    //     break;
    case AZBA:
        postfix = "azba/azba.hdf5";
        break;
    case CELLS:
        postfix = "cells/cells_055.hdf5";
        break;
    case FIBER:
        postfix = "fiber/maurer_glassfiberpolymer.hdf5";
        break;
    // case GRIESSER2022VAL:
    //     postfix = "Griesser2022-validation/Griesser2022-validation_x0y0z0.hdf5";
    //     break;
    case MOTTA2019SMALL:
        postfix = "Motta2019-small/Motta2019_x2y3z2.hdf5";
        break;
    case PA66:
        postfix = "pa66/pa66_segm.hdf5";
        break;
    case WOLNY2020:
        postfix = "Wolny2020/Wolny2020.hdf5";
        break;
    case XTMBATTERY:
        postfix = "xtm-battery/xtm-battery.hdf5";
        break;
    default:
        throw std::invalid_argument("Invalid data set " + std::to_string(data));
    }

    return config.data_base_dir / postfix;
}

inline std::string getDataOutputName(const DataSet data)
{
    switch (data)
    {
    // case ARA2016:
    //     return "Ara2016";
    case AZBA:
        return "azba";
    case CELLS:
        return "cells";
    case FIBER:
        return "fiber";
    // case GRIESSER2022VAL:
    //     return "Griesser2022-validation";
    case MOTTA2019SMALL:
        return "Motta2019-small";
    case PA66:
        return "pa66";
    case WOLNY2020:
        return "Wolny2020";
    case XTMBATTERY:
        return "xtm-battery";
    default:
        throw std::invalid_argument("Invalid dataset.");
    }
}

inline std::filesystem::path getVcfgPath(const Config& config, const DataSet data)
{
    return config.vcfg_base_dir / (getDataOutputName(data) + ".vcfg");
}



inline Config parseConfig(int argc, char** argv)
{
    Config config = {};

    TCLAP::CmdLine cmd("options", ' ', "1.0");

    TCLAP::SwitchArg verboseArg("", "verbose",
        "Verbose output", cmd, false);
    TCLAP::ValueArg<int> widthArg("x", "width",
        "Render width", false, config.render_width, "int", cmd);
    TCLAP::ValueArg<int> heightArg("y", "height",
        "Render height", false, config.render_height, "int", cmd);
    TCLAP::ValueArg<int> framesArg("f", "frames",
        "Number of frames to render", false, config.render_frames, "int", cmd);
    TCLAP::SwitchArg interactiveArg("", "interactive",
        "Interactive rendering", cmd, !config.offscreen);
    TCLAP::ValueArg<std::string> camImportArg("",
        "camera-import", "Camera import file", false,
        config.camera_import_file.string(), "path", cmd);
    TCLAP::ValueArg<std::string> camExportArg("",
        "camera-export", "Camera export file", false,
        config.camera_export_file.string(), "path", cmd);
    TCLAP::ValueArg<std::string> imgExportArg("",
        "image-dir", "Image export directory", false,
        config.image_export_dir.string(), "path", cmd);
    TCLAP::ValueArg<std::string> dataBaseArg("",
        "data-dir", "Data base directory", false,
        config.data_base_dir.string(), "path", cmd);
    TCLAP::ValueArg<std::string> vcfgBaseArg("",
            "vcfg-dir", ".vcfg base directory", false,
            config.vcfg_base_dir.string(), "path", cmd);
    TCLAP::ValueArg<std::string> resultFileArg("",
            "results-file", "Results .csv file", false,
            config.csv_result_file.string(), "path", cmd);
    TCLAP::ValueArg<int> dataSetArg("d", "data-set",
    "Data set index in [0 ... 6]", false, config.data_set, "int", cmd);

    cmd.parse(argc, argv);

    config.verbose       = verboseArg.getValue();
    config.offscreen     = interactiveArg.getValue();
    config.render_width  = widthArg.getValue();
    config.render_height = heightArg.getValue();
    config.render_frames = framesArg.getValue();
    if (camImportArg.isSet())
        config.camera_import_file = std::filesystem::path(camImportArg.getValue());
    if (camExportArg.isSet())
        config.camera_export_file = std::filesystem::path(camExportArg.getValue());
    if (imgExportArg.isSet())
        config.image_export_dir = std::filesystem::path(imgExportArg.getValue());
    if (dataBaseArg.isSet())
        config.data_base_dir = std::filesystem::path(dataBaseArg.getValue());
    if (vcfgBaseArg.isSet())
        config.vcfg_base_dir = std::filesystem::path(vcfgBaseArg.getValue());
    if (resultFileArg.isSet())
        config.csv_result_file = std::filesystem::path(resultFileArg.getValue());
    config.data_set = static_cast<DataSet>(dataSetArg.getValue());

    return config;
}
