#ifndef GIFPROCESSOR_H
#define GIFPROCESSOR_H

#include <QObject>
#include <QString>
#include <QVector>

// 压缩参数结构体
struct CompressionParams {
  int colors;
  int quality;
  double scale;
  double fps;
  QString name;
};

class GifProcessor : public QObject {
  Q_OBJECT

public:
  explicit GifProcessor(QObject *parent = nullptr);
  ~GifProcessor();

  void setInputPath(const QString &path) { m_inputPath = path; }
  void setOutputDir(const QString &dir) { m_outputDir = dir; }

public slots:
  void process();

signals:
  void progressUpdated(int percent);
  void statusMessage(const QString &message);
  void finished(const QStringList &outputFiles);
  void error(const QString &message);
  void sizeWarning(const QString &message); // 大小警告信号

private:
  CompressionParams findOptimalParams(const QString &ffmpegPath,
                                      const QString &inputPath, int cropWidth,
                                      int height, int left, double targetFps,
                                      int targetSizeMB);

  bool compressWithUnifiedParams(const QString &ffmpegPath,
                                 const QString &inputPath,
                                 const QString &outputPath, int cropWidth,
                                 int height, int left,
                                 const CompressionParams &params,
                                 int pieceNum); // 添加 pieceNum 参数

  double getGifFrameRate(const QString &ffprobePath, const QString &gifPath);
  bool verifyAlignment(const QStringList &outputFiles);
  void fixGifForSteam(const QString &gifPath);

  QString m_inputPath;
  QString m_outputDir;

  static constexpr int TARGET_SIZE_MB = 5;
};

#endif // GIFPROCESSOR_H