#ifndef CLIPMANAGER_H
#define CLIPMANAGER_H

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <vector>

struct ClipInfo {
    QString name;
    QString filepath;
    QDateTime timestamp;
    float duration;
    qint64 fileSize;
};

class ClipManager {
public:
    ClipManager();
    
    void setClipsDirectory(const QString& directory);
    QString getClipsDirectory() const { return clipsDirectory_; }
    
    // Clip management
    QStringList getClipList();
    ClipInfo getClipInfo(const QString& clipName);
    bool renameClip(const QString& oldName, const QString& newName);
    bool deleteClip(const QString& clipName);
    QString generateClipName();
    
    // File operations
    void revealInExplorer(const QString& clipName);
    
private:
    QString clipsDirectory_;
    void ensureDirectoryExists();
};

#endif // CLIPMANAGER_H