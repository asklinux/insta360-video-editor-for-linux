#pragma once

#include <QString>

class SphericalMetadata {
public:
    static bool inject(const QString &inputPath, const QString &outputPath, QString *error);
};
