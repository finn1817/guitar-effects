#include "ClipManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>

ClipManager::ClipManager()
{
#ifdef Q_OS_WIN
    clipsDirectory_ = QDir::homePath() + "/AppData/Roaming/GuitarEffectsApp/Clips";
#elif defined(Q_OS_MAC)
    clipsDirectory_ = QDir::homePath() + "/Library/Application Support/GuitarEffectsApp/Clips";
#else
    clipsDirectory_ = QDir::homePath() + "/.local/share/GuitarEffectsApp/Clips";
#endif
    
    ensureDirectoryExists();
}

void ClipManager::setClipsDirectory(const QString& directory)
{
    clipsDirectory_ = directory;
    ensureDirectoryExists();
}

void ClipManager::ensureDirectoryExists()
{
    QDir dir;
    if (!dir.exists(clipsDirectory_)) {
        dir.mkpath(clipsDirectory_);
    }
}

QStringList ClipManager::getClipList()
{
    QDir dir(clipsDirectory_);
    QStringList filters;
    filters << "*.wav";
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    QStringList clipNames;
    
    for (const QFileInfo& file : files) {
        clipNames.append(file.baseName());
    }
    
    return clipNames;
}

ClipInfo ClipManager::getClipInfo(const QString& clipName)
{
    ClipInfo info;
    QString filepath = clipsDirectory_ + "/" + clipName + ".wav";
    QFileInfo fileInfo(filepath);
    
    if (fileInfo.exists()) {
        info.name = clipName;
        info.filepath = filepath;
        info.timestamp = fileInfo.lastModified();
        info.fileSize = fileInfo.size();
        
        // Read WAV file to get duration
        QFile file(filepath);
        if (file.open(QIODevice::ReadOnly)) {
            file.seek(24); // Skip to sample rate position
            char buffer[4];
            file.read(buffer, 4);
            int sampleRate = *reinterpret_cast<int*>(buffer);
            
            file.seek(40); // Data chunk size
            file.read(buffer, 4);
            int dataSize = *reinterpret_cast<int*>(buffer);
            
            int bytesPerSample = 6; // 2 channels * 24 bits / 8
            int numSamples = dataSize / bytesPerSample;
            info.duration = static_cast<float>(numSamples) / sampleRate;
            
            file.close();
        }
    }
    
    return info;
}

bool ClipManager::renameClip(const QString& oldName, const QString& newName)
{
    QString oldPath = clipsDirectory_ + "/" + oldName + ".wav";
    QString newPath = clipsDirectory_ + "/" + newName + ".wav";
    
    QFile file(oldPath);
    return file.rename(newPath);
}

bool ClipManager::deleteClip(const QString& clipName)
{
    QString filepath = clipsDirectory_ + "/" + clipName + ".wav";
    QFile file(filepath);
    return file.remove();
}

QString ClipManager::generateClipName()
{
    QDateTime now = QDateTime::currentDateTime();
    QString baseName = "Clip_" + now.toString("yyyyMMdd_HHmmss");
    
    QString filepath = clipsDirectory_ + "/" + baseName + ".wav";
    int counter = 1;
    
    while (QFile::exists(filepath)) {
        baseName = "Clip_" + now.toString("yyyyMMdd_HHmmss") + "_" + QString::number(counter);
        filepath = clipsDirectory_ + "/" + baseName + ".wav";
        counter++;
    }
    
    return baseName;
}

void ClipManager::revealInExplorer(const QString& clipName)
{
    QString filepath = clipsDirectory_ + "/" + clipName + ".wav";
    
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer", QStringList() << "/select," << QDir::toNativeSeparators(filepath));
#elif defined(Q_OS_MAC)
    QProcess::execute("/usr/bin/osascript", QStringList() 
        << "-e" << QString("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(filepath));
    QProcess::execute("/usr/bin/osascript", QStringList() 
        << "-e" << "tell application \"Finder\" to activate");
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(clipsDirectory_));
#endif
}