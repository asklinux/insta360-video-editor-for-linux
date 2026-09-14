#include "SphericalMetadata.h"

#include <QFile>

static quint32 readU32BE(const QByteArray &data)
{
    return (static_cast<quint8>(data[0]) << 24)
        | (static_cast<quint8>(data[1]) << 16)
        | (static_cast<quint8>(data[2]) << 8)
        | static_cast<quint8>(data[3]);
}

static void appendU32BE(QByteArray *data, quint32 value)
{
    data->append(static_cast<char>((value >> 24) & 0xff));
    data->append(static_cast<char>((value >> 16) & 0xff));
    data->append(static_cast<char>((value >> 8) & 0xff));
    data->append(static_cast<char>(value & 0xff));
}

bool SphericalMetadata::inject(const QString &inputPath, const QString &outputPath, QString *error)
{
    QFile input(inputPath);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = input.errorString();
        }
        return false;
    }

    const QByteArray source = input.readAll();
    if (source.size() < 12) {
        if (error) {
            *error = QStringLiteral("MP4 terlalu kecil atau rosak.");
        }
        return false;
    }

    const quint32 firstBoxSize = readU32BE(source.left(4));
    if (firstBoxSize < 8 || firstBoxSize > static_cast<quint32>(source.size())
        || source.mid(4, 4) != QByteArrayLiteral("ftyp")) {
        if (error) {
            *error = QStringLiteral("Fail output bukan MP4 yang sah.");
        }
        return false;
    }

    const QByteArray xmp =
        "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
        "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
        "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
        "<rdf:Description rdf:about=\"\" xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\" "
        "GSpherical:Spherical=\"true\" GSpherical:Stitched=\"true\" "
        "GSpherical:StitchingSoftware=\"Insta360 Studio Editor\" "
        "GSpherical:ProjectionType=\"equirectangular\"/>\n"
        "</rdf:RDF>\n"
        "</x:xmpmeta>\n"
        "<?xpacket end=\"w\"?>";

    QByteArray uuidBox;
    appendU32BE(&uuidBox, static_cast<quint32>(8 + 16 + xmp.size()));
    uuidBox.append("uuid", 4);
    const unsigned char uuidBytes[16] = {
        0xff, 0xcc, 0x82, 0x63, 0xf8, 0x55, 0x4a, 0x93,
        0x88, 0x14, 0x58, 0x7a, 0x02, 0x52, 0x1f, 0xdd
    };
    uuidBox.append(reinterpret_cast<const char *>(uuidBytes), 16);
    uuidBox.append(xmp);

    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = output.errorString();
        }
        return false;
    }

    output.write(source.constData(), firstBoxSize);
    output.write(uuidBox);
    output.write(source.constData() + firstBoxSize, source.size() - firstBoxSize);
    return true;
}
