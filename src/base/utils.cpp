#include "base/utils.h"
#include "windows/dialogs.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QImageWriter>
#include <QProcess>
#include <QScreen>
#include <QtConcurrent/QtConcurrent>

QString Utils::capitalize(QString string)
{
    if (!string.isEmpty()) {
        string[0] = string[0].toUpper();
    }
    return string;
}

int Utils::roundToNearest(int number, QList<int> numbers)
{
    if (numbers.isEmpty()) {
        return number;
    }
    std::sort(numbers.begin(), numbers.end());
    const int first = numbers.at(0);
    const int last = numbers.at(numbers.size() - 1);
    if (number <= first) {
        return first;
    }
    if (number >= last) {
        return last;
    }
    for (int i = 0; i < numbers.count() - 1; ++i) {
        const int prev = numbers.at(i);
        const int next = numbers.at(i + 1);
        if (number < prev || number > next) {
            // Number is not in the range; continue loop
            continue;
        }
        const qreal middle = (prev + next) / 2.0;
        if (number <= middle) {
            return prev;
        } else {
            return next;
        }
    }
    return number;
}

bool Utils::isDarkTheme()
{
    return QPalette().color(QPalette::Base).lightness() < 127;
}

bool Utils::isLightTheme()
{
    return !isDarkTheme();
}

bool Utils::explore(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    const QFileInfo fileInfo(path);
#if defined(Q_OS_WIN)
    const QString nativePath = QDir::toNativeSeparators(fileInfo.canonicalFilePath());
    const QString argument = fileInfo.isDir() ? nativePath : QString("/select,%1").arg(nativePath);
    return QProcess::startDetached("explorer.exe", {argument});
#elif defined(Q_OS_MACOS)
    QStringList arguments;
    const QString action = fileInfo.isDir() ? "open" : "reveal";
    arguments << "-e"
              << QString("tell application \"Finder\" to %1 POSIX file \"%2\"").arg(action, fileInfo.canonicalFilePath());
    QProcess::execute(QLatin1String("/usr/bin/osascript"), arguments);
    arguments.clear();
    arguments << "-e"
              << QString("tell application \"Finder\" to activate");
    QProcess::execute(QLatin1String("/usr/bin/osascript"), arguments);
    return true;
#else
    const QString directory = fileInfo.isDir() ? fileInfo.canonicalFilePath() : fileInfo.canonicalPath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
#endif
}

void Utils::rmdir(const QString &path, bool recursive)
{
    if (!recursive) {
        QDir().rmdir(path);
    } else if (!path.isEmpty()) {
        QtConcurrent::run([=]() {
            QDir(path).removeRecursively();
        });
    }
}

namespace
{
    bool copy(const QString &src, const QString &dst)
    {
        if (src.isEmpty() || dst.isEmpty() || !QFile::exists(src)) {
            qWarning() << "Could not copy file: invalid path to file(s).";
            return false;
        }
        if (QFileInfo(src) == QFileInfo(dst)) {
            qWarning() << "Could not copy file: source and destination paths are identical.";
            return false;
        }
        const QString srcSuffix = QFileInfo(src).suffix();
        const QString dstSuffix = QFileInfo(dst).suffix();
        const bool sameFormats = !QString::compare(srcSuffix, dstSuffix, Qt::CaseInsensitive);
        const bool isSrcReadableImage = Utils::isImageReadable(src);
        const bool isDstWritableImage = Utils::isImageWritable(dst);
        if (!(isSrcReadableImage && isDstWritableImage && !sameFormats)) {
            QFile::remove(dst);
            return QFile::copy(src, dst);
        } else {
            return QImage(src).save(dst);
        }
    }
}

bool Utils::copyFile(const QString &src, QWidget *parent)
{
    return copyFile(src, QString(), parent);
}

bool Utils::copyFile(const QString &src, QString dst, QWidget *parent)
{
    if (dst.isNull()) {
        dst = isImageReadable(src)
            ? Dialogs::getSaveImageFilename(src, parent)
            : Dialogs::getSaveFilename(src, parent);
    }
    if (dst.isEmpty()) {
        return false;
    }
    if (!copy(src, dst)) {
        QMessageBox::warning(parent, QString(), qApp->translate("Utils", "Could not save the file."));
        return false;
    }
    return true;
}

bool Utils::replaceFile(const QString &what, QWidget *parent)
{
    return replaceFile(what, QString(), parent);
}

bool Utils::replaceFile(const QString &what, QString with, QWidget *parent)
{
    if (with.isNull()) {
        with = isImageWritable(what)
            ? Dialogs::getOpenImageFilename(what, parent)
            : Dialogs::getOpenFilename(what, parent);
    }
    if (with.isEmpty() || QFileInfo(with) == QFileInfo(what)) {
        return false;
    }
    if (!copy(with, what)) {
        QMessageBox::warning(parent, QString(), qApp->translate("Utils", "Could not replace the file."));
        return false;
    }
    return true;
}

QString Utils::normalizePath(QString path)
{
    return path.replace(QRegularExpression("^\\/+[\\.\\./*]*\\/*|$"), "/");
}

QString Utils::toAbsolutePath(const QString &path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path)) {
        return path;
    } else {
        return QDir::cleanPath(qApp->applicationDirPath() + '/' + path);
    }
}

bool Utils::isImageReadable(const QString &path)
{
    const QString extension = QFileInfo(path).suffix();
    return QImageReader::supportedImageFormats().contains(extension.toLocal8Bit());
}

bool Utils::isImageWritable(const QString &path)
{
    const QString extension = QFileInfo(path).suffix();
    return QImageWriter::supportedImageFormats().contains(extension.toLocal8Bit());
}

QPixmap Utils::iconToPixmap(const QIcon &icon)
{
    const auto sizes = icon.availableSizes();
    const QSize size = !sizes.isEmpty() ? sizes.first() : QSize();
    return icon.pixmap(size);
}

QString Utils::getAppTitle()
{
    return APPLICATION;
}

QString Utils::getAppVersion()
{
    return VERSION;
}

QString Utils::getAppTitleSlug()
{
    return getAppTitle().toLower().replace(' ', '-');
}

QString Utils::getAppTitleAndVersion()
{
    return QString("%1 v%2").arg(getAppTitle(), getAppVersion());
}

qreal Utils::getScaleFactor()
{
#ifndef Q_OS_MACOS
    const qreal dpi = qApp->primaryScreen()->logicalDotsPerInch();
    return dpi / 100.0;
#else
    return 1;
#endif
}

int Utils::scale(int value)
{
    return static_cast<int>(value * getScaleFactor());
}

qreal Utils::scale(qreal value)
{
    return value * getScaleFactor();
}

QSize Utils::scale(int width, int height)
{
    return QSize(width, height) * getScaleFactor();
}

QString Utils::getTemporaryPath(const QString &subdirectory)
{
#ifndef PORTABLE
    const QString path = QString("%1/%2/%3").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), getAppTitleSlug(), subdirectory);
#else
    const QString path = QString("%1/data/temp/%2").arg(qApp->applicationDirPath(), subdirectory);
#endif
    return QDir::cleanPath(path);
}

QString Utils::getLocalConfigPath(const QString &subdirectory)
{
#ifndef PORTABLE
    const QString path = QString("%1/%2/%3").arg(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation), getAppTitleSlug(), subdirectory);
#else
    const QString path = QString("%1/data/%2").arg(qApp->applicationDirPath(), subdirectory);
#endif
    return QDir::cleanPath(path);
}

QString Utils::getSharedPath(const QString &resource)
{
#ifndef Q_OS_LINUX
    const QString path = QString("%1/%2").arg(qApp->applicationDirPath(), resource);
#else
    const QString path = QString("%1/../share/%2/%3").arg(qApp->applicationDirPath(), getAppTitleSlug(), resource);
#endif
    return QDir::cleanPath(path);
}

QString Utils::getBinaryPath(const QString &executable)
{
#ifdef Q_OS_WIN
    QString path = getSharedPath("tools/" + executable);
#else
    const QString path = QString("%1/%2").arg(qApp->applicationDirPath(), executable);
#endif

    QFileInfo fileInfo(path);
#ifdef Q_OS_WIN
    if (fileInfo.suffix().isEmpty()) {
        path.append(".exe");
        fileInfo.setFile(path);
    }
#endif
    return fileInfo.exists() ? path : fileInfo.fileName();
}

QIcon Utils::getLocaleFlag(const QLocale &locale)
{
    const QStringList localeSegments = locale.name().split('_');
    if (localeSegments.count() <= 1) {
        return QPixmap();
    }
    const QString languageCode = localeSegments.at(0);
    if (languageCode == "ckb") {
        // Override the flag icon for Kurdistan
        return QIcon::fromTheme(QStringLiteral("flag-ku"));
    }
    const QString countryCode = localeSegments.at(1).toLower();
    return QIcon::fromTheme(QString("flag-%1").arg(countryCode));
}

QString Utils::getWebsiteUrl()
{
    return QString("https://qwertycube.com/%1/").arg(getAppTitleSlug());
}

QString Utils::getWebsiteUtmUrl()
{
    return QString("%1#utm_source=%2&utm_medium=application").arg(getWebsiteUrl(), getAppTitleSlug());
}

QString Utils::getUpdateUrl()
{
    return QString("%1#utm_campaign=update&utm_source=%2&utm_medium=application").arg(getWebsiteUrl(), getAppTitleSlug());
}

QString Utils::getRepositoryUrl()
{
    return QString("https://github.com/kefir500/%1").arg(getAppTitleSlug());
}

QString Utils::getIssuesUrl()
{
    return getRepositoryUrl() + "/issues";
}

QString Utils::getTranslationsUrl()
{
    return QString("https://www.transifex.com/qwertycube/%1/").arg(getAppTitleSlug());
}

QString Utils::getDonationsUrl()
{
    return QString("https://qwertycube.com/donate/#utm_campaign=donate&utm_source=%1&utm_medium=application").arg(getAppTitleSlug());
}

QString Utils::getBlogPostUrl(const QString &slug)
{
    return QString("%1blog/%2/").arg(getWebsiteUrl(), slug);
}

QString Utils::getVersionInfoUrl()
{
    return QString("%1versions.json").arg(getWebsiteUrl());
}

QString Utils::getDonorsInfoUrl()
{
    return QString("https://qwertycube.com/donate/donations.json");
}

QString Utils::getAndroidCodename(int api)
{
    // Read more: https://source.android.com/setup/start/build-numbers
    switch (api) {
    case ANDROID_3:
        return "1.5 - Cupcake";
    case ANDROID_4:
        return "1.6 - Donut";
    case ANDROID_5:
        return "2.0 - Eclair";
    case ANDROID_6:
        return "2.0.1 - Eclair";
    case ANDROID_7:
        return "2.1 - Eclair";
    case ANDROID_8:
        return "2.2.x - Froyo";
    case ANDROID_9:
        return "2.3 - 2.3.2 - Gingerbread";
    case ANDROID_10:
        return "2.3.3 - 2.3.7 - Gingerbread";
    case ANDROID_11:
        return "3.0 - Honeycomb";
    case ANDROID_12:
        return "3.1 - Honeycomb";
    case ANDROID_13:
        return "3.2.x - Honeycomb";
    case ANDROID_14:
        return "4.0.1 - 4.0.2 - Ice Cream Sandwich";
    case ANDROID_15:
        return "4.0.3 - 4.0.4 - Ice Cream Sandwich";
    case ANDROID_16:
        return "4.1.x - Jelly Bean";
    case ANDROID_17:
        return "4.2.x - Jelly Bean";
    case ANDROID_18:
        return "4.3.x - Jelly Bean";
    case ANDROID_19:
        return "4.4 - 4.4.4 - KitKat";
    case ANDROID_20:
        return "4.4 - 4.4.4 - KitKat Wear";
    case ANDROID_21:
        return "5.0 - Lollipop";
    case ANDROID_22:#include "apk/apkcloner.h"
#include <QDirIterator>
#include <QtConcurrent/QtConcurrent>

ApkCloner::ApkCloner(const QString &contentsPath, const QString &originalPackageName, const QString &newPackageName, QObject *parent)
    : QObject(parent)
    , contentsPath(contentsPath)
    , originalPackageName(originalPackageName)
    , newPackageName(newPackageName)
{
    newPackagePath = newPackageName;
    newPackagePath.replace('.', '/');

    originalPackagePath = originalPackageName;
    originalPackagePath.replace('.', '/');
}

void ApkCloner::start()
{
    QtConcurrent::run([this]() {
        emit started();

        // Update references in resources:

        const auto resourcesPath = contentsPath + "/res/";
        QDirIterator files(resourcesPath, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString path(files.next());
            emit progressed(tr("Updating resource references..."), path.mid(contentsPath.size() + 1));

            QFile file(path);
            if (file.open(QFile::ReadWrite)) {
                const QString data(file.readAll());
                QString newData(data);
                newData.replace(originalPackageName, newPackageName);
                if (newData != data) {
                    file.resize(0);
                    file.write(newData.toUtf8());
                }
                file.close();
            }
        }

        // Update references in AndroidManifest.xml:

        const auto manifestPath = contentsPath + "/AndroidManifest.xml";
        emit progressed(tr("Updating AndroidManifest.xml..."), manifestPath.mid(contentsPath.size() + 1));

        QFile file(manifestPath);
        if (file.open(QFile::ReadWrite)) {
            const QString data(file.readAll());
            QString newData(data);
            newData.replace(originalPackageName, newPackageName);
            if (newData != data) {
                file.resize(0);
                file.write(newData.toUtf8());
            }
            file.close();
        }

       //add function to port an apk from base apk to port apk 
       
         void ApkCloner::portApk(const QString &contentsPath, const QString &originalPackageName, const QString &newPackageName, QObject *parent)
    : QObject(parent)
    , contentsPath(contentsPath)
    , originalPackageName(originalPackageName)
    , newPackageName(newPackageName)
{
    newPackagePath = newPackageName;
    newPackagePath.replace('.', '/');
    originalPackagePath = originalPackageName;
    originalPackagePath.replace('.', '/');

    //port apk theme from one apk to another apk
    const auto portPath = contentsPath + "/res/";
    QDirIterator files(portPath, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString path(files.next());
        emit progressed(tr("Updating resource references..."), path.mid(contentsPath.size() + 1));

        QFile file(path);
        if (file.open(QFile::ReadWrite)) {
            const QString data(file.readAll());
            QString newData(data);
            newData.replace(originalPackageName, newPackageName);
            if (newData != data) {
                file.resize(0);
                file.write(newData.toUtf8());
            }
            file.close();
        }
    }
}




        const auto smaliDirs = QDir(contentsPath).entryList({"smali*"}, QDir::Dirs);
        for (const auto &smaliDir : smaliDirs) {

            const auto smaliPath = QString("%1/%2/").arg(contentsPath, smaliDir);

            // Update references in smali:

            QDirIterator files(smaliPath, QDir::Files, QDirIterator::Subdirectories);
            while (files.hasNext()) {
                const QString path(files.next());
                //: "Smali" is the name of the tool/format, don't translate it.
                emit progressed(tr("Updating Smali references..."), path.mid(contentsPath.size() + 1));

                QFile file(path);
                if (file.open(QFile::ReadWrite)) {
                    const QString data(file.readAll());
                    QString newData(data);
                    newData.replace('L' + originalPackagePath, 'L' + newPackagePath);
                    newData.replace(originalPackageName, newPackageName);
                    if (newData != data) {
                        file.resize(0);
                        file.write(newData.toUtf8());
                    }
                    file.close();
                }
            }

            // Update directory structure:

            emit progressed(tr("Updating directory structure..."), smaliDir);

            const auto fullPackagePath = smaliPath + newPackagePath;
            const auto fullOriginalPackagePath = smaliPath + originalPackagePath;
            if (!QDir().exists(fullOriginalPackagePath)) {
                continue;
            }
            QDir().mkpath(QFileInfo(fullPackagePath).path());
            if (!QDir().rename(fullOriginalPackagePath, fullPackagePath)) {
                emit finished(false);
                return;
            }
        }
\
        emit finished(true);
    });
}

        return "5.1 - Lollipop";
    case ANDROID_23:
        return "6.0 - Marshmallow";
    case ANDROID_24:
        return "7.0 - Nougat";
    case ANDROID_25:
        return "7.1 - Nougat";
    case ANDROID_26:
        return "8.0 - Oreo";
    case ANDROID_27:
        return "8.1 - Oreo";
    case ANDROID_28:
        return "9.0 - Pie";
    case ANDROID_29:
        return "Android 10";
    case ANDROID_30:
        return "Android 11";
    case ANDROID_31:
        return "Android 12";
    case ANDROID_32:
        return "Android 12L";
    case ANDROID_33:
        return "Android 13";
    }
    return QString();
}

bool Utils::isDrawableResource(const QFileInfo &file)
{
    // Read more: https://developer.android.com/guide/topics/resources/drawable-resource.html
    const QStringList drawableFormats = {"png", "jpg", "jpeg", "gif", "xml", "webp"};
    return drawableFormats.contains(file.suffix());
}
