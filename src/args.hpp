#pragma once

#include <optional>
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
    ARA2016 = 7,
    GRIESSER2022VALIDATION = 8,
};
constexpr int DATA_SET_COUNT = 9;

struct Config
{
    bool verbose = false;
    int render_width = 1920;
    int render_height = 1080;
    int render_frames = 300;
    bool offscreen = false;
    std::filesystem::path camera_import_file = {};
    std::filesystem::path camera_export_file = "./camera.cam";
    std::filesystem::path image_export_dir = "./";
    std::optional<std::filesystem::path> image_export_override_file = {};
    std::filesystem::path data_base_dir = "./";
    std::filesystem::path vcfg_base_dir = "./";
    std::optional<std::filesystem::path> vcfg_override_file = {};
    std::filesystem::path csv_result_file = "./results.csv";
    // note: Griesser2022-sample, Motta2019, H01-wm, H01-bloodvessel, liconn unavailable: exceed 64 GB RAM.
    DataSet data_set = AZBA;
    bool exit_with_data_count = false;  ///< returns the data set count and exits
};


// TODO: separate all code paths / configs for the Volcanite evaluation from the more general code
std::filesystem::path getDataInputPath(const Config& config, const DataSet data)
{
    std::filesystem::path postfix = {};
    switch (data)
    {
    case ARA2016:
        postfix = "Ara2016/Ara2016_full.hdf5";
        break;
    case AZBA:
        postfix = "azba/azba.hdf5";
        break;
    case CELLS:
        postfix = "cells/cells_055.hdf5";
        break;
    case FIBER:
        postfix = "fiber/maurer_glassfiberpolymer.hdf5";
        break;
    case GRIESSER2022VALIDATION:
        postfix = "Griesser2022-validation/Griesser2022-validation_full.hdf5";
        break;
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
    case ARA2016:
        return "Ara2016";
    case AZBA:
        return "azba";
    case CELLS:
        return "cells";
    case FIBER:
        return "fiber";
    case GRIESSER2022VALIDATION:
        return "Griesser2022-validation";
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
    if (config.vcfg_override_file.has_value())
        return config.vcfg_override_file.value();
    else
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
    TCLAP::ValueArg<std::string> imgExportOverrideFileArg("",
        "image-output-file", "Image output file (overrides auto select from image-dir)",
        false, "", "path", cmd);
    TCLAP::ValueArg<std::string> dataBaseArg("",
        "data-dir", "Data base directory", false,
        config.data_base_dir.string(), "path", cmd);
    TCLAP::ValueArg<std::string> vcfgBaseArg("",
            "vcfg-dir", ".vcfg base directory", false,
            config.vcfg_base_dir.string(), "path", cmd);
    TCLAP::ValueArg<std::string> vcfgOverrideFileArg("",
            "vcfg-file", ".vcfg configuration file (overrides auto select from vcfg-dir)", false,
            "", "path", cmd);
    TCLAP::ValueArg<std::string> resultFileArg("",
            "results-file", "Results .csv file", false,
            config.csv_result_file.string(), "path", cmd);
    TCLAP::ValueArg<int> dataSetArg("d", "data-set",
    "Data set index in [0 ... 6]", false, config.data_set, "int", cmd);
    TCLAP::SwitchArg listDataArg("", "list-data",
        "Prints all data set IDs to the console and exits. Returns the data set count.", cmd, false);

    cmd.parse(argc, argv);

    if (listDataArg.isSet())
    {
        std::cout << "Available data sets:" << std::endl;
        for (int i = 0; i < DATA_SET_COUNT; i++)
            std::cout << i << ": " << getDataOutputName(static_cast<DataSet>(i)) << std::endl;
        std::cout << std::endl;
        config.exit_with_data_count = true;
    }

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
    if (imgExportOverrideFileArg.isSet())
        config.image_export_override_file = std::filesystem::path(imgExportOverrideFileArg.getValue());
    if (dataBaseArg.isSet())
        config.data_base_dir = std::filesystem::path(dataBaseArg.getValue());
    if (vcfgBaseArg.isSet())
        config.vcfg_base_dir = std::filesystem::path(vcfgBaseArg.getValue());
    if (vcfgOverrideFileArg.isSet())
        config.vcfg_override_file = std::filesystem::path(vcfgOverrideFileArg.getValue());
    if (resultFileArg.isSet())
        config.csv_result_file = std::filesystem::path(resultFileArg.getValue());
    config.data_set = static_cast<DataSet>(dataSetArg.getValue());

    return config;
}
