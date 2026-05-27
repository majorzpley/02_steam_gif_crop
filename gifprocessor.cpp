#include "gifprocessor.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>

GifProcessor::GifProcessor(QObject *parent) : QObject(parent) {}

GifProcessor::~GifProcessor() {}

double GifProcessor::getGifFrameRate(const QString &ffprobePath,
                                     const QString &gifPath) {
  QStringList probeArgs;
  probeArgs << "-v" << "error";
  probeArgs << "-select_streams" << "v:0";
  probeArgs << "-show_entries" << "stream=r_frame_rate";
  probeArgs << "-of" << "default=noprint_wrappers=1:nokey=1";
  probeArgs << gifPath;

  QProcess probe;
  probe.start(ffprobePath, probeArgs);

  if (!probe.waitForFinished(5000)) {
    return 10.0;
  }

  QString output = probe.readAllStandardOutput().trimmed();

  if (output.contains('/')) {
    QStringList parts = output.split('/');
    if (parts.size() == 2) {
      double num = parts[0].toDouble();
      double den = parts[1].toDouble();
      if (den > 0) {
        return num / den;
      }
    }
  }

  return output.toDouble();
}

bool GifProcessor::verifyAlignment(const QStringList &outputFiles) {
  if (outputFiles.size() != 5) {
    return false;
  }

  QString ffprobePath = QCoreApplication::applicationDirPath() + "/ffprobe.exe";
  if (!QFile::exists(ffprobePath)) {
    ffprobePath = QDir::current().absolutePath() + "/ffprobe.exe";
  }
  if (!QFile::exists(ffprobePath)) {
    ffprobePath = "ffprobe";
  }

  QVector<int> frameCounts;
  QVector<double> fpsList;

  for (const QString &gifPath : outputFiles) {
    QStringList args;
    args << "-v" << "error";
    args << "-select_streams" << "v:0";
    args << "-show_entries" << "stream=nb_frames";
    args << "-of" << "default=noprint_wrappers=1:nokey=1";
    args << gifPath;

    QProcess probe;
    probe.start(ffprobePath, args);
    probe.waitForFinished();
    int frameCount = probe.readAllStandardOutput().trimmed().toInt();
    frameCounts.append(frameCount);

    double fps = getGifFrameRate(ffprobePath, gifPath);
    fpsList.append(fps);
  }

  bool allMatch = true;
  int baseFrameCount = frameCounts[0];
  double baseFps = fpsList[0];

  for (int i = 1; i < 5; ++i) {
    if (frameCounts[i] != baseFrameCount || qAbs(fpsList[i] - baseFps) > 0.1) {
      allMatch = false;
      break;
    }
  }

  return allMatch;
}

CompressionParams GifProcessor::findOptimalParams(const QString &ffmpegPath,
                                                  const QString &inputPath,
                                                  int cropWidth, int height,
                                                  int left, double targetFps,
                                                  int targetSizeMB) {
  // 更激进的压缩策略
  QVector<CompressionParams> strategies = {
      // 保持原始帧率
      {128, 18, 1.00, targetFps, "1. 中等质量"},
      {96, 20, 0.95, targetFps, "2. 标准压缩"},
      {64, 22, 0.90, targetFps, "3. 高压缩"},
      {48, 24, 0.85, targetFps, "4. 高压缩+"},

      // 降低帧率到 12
      {48, 24, 0.85, 12.0, "5. 降帧(12fps)"},
      {32, 26, 0.80, 12.0, "6. 强降帧(12fps)"},

      // 降低帧率到 10
      {32, 26, 0.80, 10.0, "7. 降帧(10fps)"},
      {24, 28, 0.75, 10.0, "8. 强降帧(10fps)"},

      // 降低帧率到 8
      {24, 28, 0.75, 8.0, "9. 降帧(8fps)"},
      {16, 30, 0.70, 8.0, "10. 强降帧(8fps)"},

      // 降低帧率到 6
      {16, 30, 0.65, 6.0, "11. 降帧(6fps)"},
      {8, 31, 0.60, 6.0, "12. 强降帧(6fps)"},

      // 最终保底
      {8, 31, 0.50, 5.0, "13. 保底压缩"},
      {4, 31, 0.40, 4.0, "14. 极限压缩"}};

  QTemporaryDir tempDir;
  if (!tempDir.isValid()) {
    return strategies.last();
  }

  for (const auto &strategy : strategies) {
    qDebug() << "测试策略:" << strategy.name;

    QString filterChain =
        QString("crop=%1:%2:%3:0").arg(cropWidth).arg(height).arg(left);

    if (strategy.fps > 0 && strategy.fps < 20) {
      filterChain += QString(",fps=%1").arg(strategy.fps);
    }

    if (strategy.scale < 0.99) {
      int newWidth = (int)(cropWidth * strategy.scale);
      int newHeight = (int)(height * strategy.scale);
      if (newWidth < 1)
        newWidth = 1;
      if (newHeight < 1)
        newHeight = 1;
      filterChain += QString(",scale=%1:%2").arg(newWidth).arg(newHeight);
    }

    QString tempPath = tempDir.path() + "/test.gif";

    QStringList args;
    args << "-y";
    args << "-i" << inputPath;
    args << "-vf" << filterChain;
    args << "-loop" << "0";
    args << tempPath;

    qDebug() << "测试命令:" << ffmpegPath << args.join(" ");

    QProcess process;
    process.start(ffmpegPath, args);

    if (!process.waitForFinished(60000)) {
      qDebug() << "策略超时:" << strategy.name;
      continue;
    }

    if (process.exitCode() != 0) {
      qDebug() << "策略失败，退出码:" << process.exitCode();
      continue;
    }

    QFileInfo info(tempPath);
    if (!info.exists() || info.size() == 0) {
      qDebug() << "策略生成空文件";
      continue;
    }

    double sizeMB = info.size() / (1024.0 * 1024.0);
    qDebug() << "策略成功，大小:" << sizeMB << "MB";

    if (sizeMB <= targetSizeMB) {
      qDebug() << "选择策略:" << strategy.name;
      return strategy;
    }
  }

  qDebug() << "使用保底策略";
  return strategies.last();
}

bool GifProcessor::compressWithUnifiedParams(
    const QString &ffmpegPath, const QString &inputPath,
    const QString &outputPath, int cropWidth, int height, int left,
    const CompressionParams &params, int pieceNum) {
  QString filterChain =
      QString("crop=%1:%2:%3:0").arg(cropWidth).arg(height).arg(left);

  if (params.fps > 0 && params.fps < 20) {
    filterChain += QString(",fps=%1").arg(params.fps);
  }

  if (params.scale < 0.99) {
    int newWidth = (int)(cropWidth * params.scale);
    int newHeight = (int)(height * params.scale);
    if (newWidth < 1)
      newWidth = 1;
    if (newHeight < 1)
      newHeight = 1;
    filterChain += QString(",scale=%1:%2").arg(newWidth).arg(newHeight);
  }

  qDebug() << "滤镜链:" << filterChain;

  QStringList args;
  args << "-y";
  args << "-i" << inputPath;
  args << "-vf" << filterChain;
  args << "-loop" << "0";
  args << outputPath;

  qDebug() << "FFmpeg 命令:" << ffmpegPath << args.join(" ");

  QProcess process;
  process.start(ffmpegPath, args);

  if (!process.waitForFinished(120000)) {
    process.kill();
    emit error("FFmpeg 执行超时");
    return false;
  }

  int exitCode = process.exitCode();
  QString stdErr = process.readAllStandardError();

  qDebug() << "FFmpeg 退出码:" << exitCode;

  if (exitCode != 0) {
    qDebug() << "错误输出:" << stdErr.left(500);
    emit error(QString("FFmpeg 失败，退出码: %1").arg(exitCode));
    return false;
  }

  if (!QFile::exists(outputPath)) {
    emit error("FFmpeg 没有生成输出文件");
    return false;
  }

  QFileInfo info(outputPath);
  if (info.size() == 0) {
    emit error("FFmpeg 生成的文件大小为 0");
    return false;
  }

  double sizeMB = info.size() / (1024.0 * 1024.0);
  qDebug() << "生成成功:" << outputPath << "大小:" << sizeMB << "MB";

  if (sizeMB > TARGET_SIZE_MB) {
    QString warning = QString("⚠️ 第%1块 GIF 大小为 %2 MB，超过 %3 MB 限制！")
                          .arg(pieceNum)
                          .arg(sizeMB, 0, 'f', 2)
                          .arg(TARGET_SIZE_MB);
    emit sizeWarning(warning);
  }

  // Steam 修复：修改 GIF 末尾标记
  fixGifForSteam(outputPath);

  return true;
}

void GifProcessor::process() {
  qDebug() << "开始处理:" << m_inputPath;

  QDir dir;
  if (!dir.mkpath(m_outputDir)) {
    emit error("无法创建输出目录");
    return;
  }

  emit statusMessage("正在分析 GIF...");

  QString ffprobePath = QCoreApplication::applicationDirPath() + "/ffprobe.exe";
  if (!QFile::exists(ffprobePath)) {
    ffprobePath = QDir::current().absolutePath() + "/ffprobe.exe";
  }
  if (!QFile::exists(ffprobePath)) {
    ffprobePath = "ffprobe";
  }

  // 获取原始 GIF 信息
  QStringList probeArgs;
  probeArgs << "-v" << "error";
  probeArgs << "-select_streams" << "v:0";
  probeArgs << "-show_entries" << "stream=r_frame_rate";
  probeArgs << "-of" << "default=noprint_wrappers=1:nokey=1";
  probeArgs << m_inputPath;

  QProcess probe;
  probe.start(ffprobePath, probeArgs);
  probe.waitForFinished();

  QString output = probe.readAllStandardOutput().trimmed();
  double originalFps = 16.67;
  if (output.contains('/')) {
    QStringList parts = output.split('/');
    if (parts.size() == 2) {
      originalFps = parts[0].toDouble() / parts[1].toDouble();
    }
  } else if (!output.isEmpty()) {
    originalFps = output.toDouble();
  }

  double targetFps = originalFps;
  if (originalFps > 15)
    targetFps = 15;
  else if (originalFps < 10)
    targetFps = 10;

  // 获取原始尺寸
  QStringList sizeArgs;
  sizeArgs << "-v" << "error";
  sizeArgs << "-select_streams" << "v:0";
  sizeArgs << "-show_entries" << "stream=width,height";
  sizeArgs << "-of" << "default=noprint_wrappers=1:nokey=1";
  sizeArgs << m_inputPath;

  QProcess sizeProbe;
  sizeProbe.start(ffprobePath, sizeArgs);
  sizeProbe.waitForFinished();

  QString sizeOutput = sizeProbe.readAllStandardOutput();
  QStringList sizeLines = sizeOutput.split('\n', Qt::SkipEmptyParts);

  int width = 1000, height = 651;
  if (sizeLines.size() >= 2) {
    width = sizeLines[0].toInt();
    height = sizeLines[1].toInt();
  }

  // ========== 计算带间隔的切割位置 ==========
  const int GAP = 2; // 间隔 2 像素

  // 原始每块宽度（不含间隔）
  int originalPieceWidth = width / 5;

  // 实际切割宽度 = 原始宽度 - 间隔（除了最后一块不需要减右边间隔）
  // 第0块: 从 0 到 originalPieceWidth - GAP
  // 第1块: 从 originalPieceWidth 到 originalPieceWidth*2 - GAP
  // 第2块: 从 originalPieceWidth*2 到 originalPieceWidth*3 - GAP
  // 第3块: 从 originalPieceWidth*3 到 originalPieceWidth*4 - GAP
  // 第4块: 从 originalPieceWidth*4 到 width (最后一块取到底，不留右边间隔)

  qDebug() << "原始尺寸:" << width << "x" << height;
  qDebug() << "每块原始宽度:" << originalPieceWidth;
  qDebug() << "间隔:" << GAP << "像素";

  // 存储每块的切割参数 [left, width]
  QVector<QPair<int, int>> pieceCuts;
  for (int piece = 0; piece < 5; ++piece) {
    int left = piece * originalPieceWidth;
    int cutWidth;

    if (piece == 4) {
      // 最后一块：从左边起始位置到图片末尾
      cutWidth = width - left;
    } else {
      // 前面4块：每块减去右侧的 GAP 像素
      cutWidth = originalPieceWidth - GAP;
    }

    pieceCuts.append(qMakePair(left, cutWidth));

    qDebug() << QString("第%1块: left=%2, width=%3, right=%4")
                    .arg(piece + 1)
                    .arg(left)
                    .arg(cutWidth)
                    .arg(left + cutWidth);
  }

  QString ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
  if (!QFile::exists(ffmpegPath)) {
    ffmpegPath = QDir::current().absolutePath() + "/ffmpeg.exe";
  }
  if (!QFile::exists(ffmpegPath)) {
    emit error("找不到 ffmpeg.exe");
    return;
  }

  // ========== 第一步：找出最难压缩的块 ==========
  emit statusMessage("正在分析各块压缩难度...");

  QVector<double> pieceSizes;

  for (int piece = 0; piece < 5; ++piece) {
    int left = pieceCuts[piece].first;
    int cropW = pieceCuts[piece].second;

    emit statusMessage(QString("分析第 %1/5 块...").arg(piece + 1));

    // 使用一个中等压缩级别测试文件大小
    CompressionParams testParams = {96, 20, 0.90, targetFps, "测试"};

    QString testPath =
        QDir::tempPath() + QString("/test_piece_%1.gif").arg(piece);

    bool success =
        compressWithUnifiedParams(ffmpegPath, m_inputPath, testPath, cropW,
                                  height, left, testParams, piece + 1);

    if (success) {
      QFileInfo info(testPath);
      double sizeMB = info.size() / (1024.0 * 1024.0);
      pieceSizes.append(sizeMB);
      qDebug() << "第" << piece + 1 << "块测试大小:" << sizeMB << "MB";
      QFile::remove(testPath);
    } else {
      pieceSizes.append(99.0);
    }
  }

  // 找出最大的块
  int hardestPiece = 0;
  double maxSize = pieceSizes[0];
  for (int i = 1; i < 5; ++i) {
    if (pieceSizes[i] > maxSize) {
      maxSize = pieceSizes[i];
      hardestPiece = i;
    }
  }

  qDebug() << "最难压缩的块: 第" << hardestPiece + 1 << "块";
  emit statusMessage(
      QString("以第 %1 块为基准寻找压缩参数...").arg(hardestPiece + 1));

  // ========== 第二步：用最难压缩的块寻找合适的统一参数 ==========
  int hardestLeft = pieceCuts[hardestPiece].first;
  int hardestCropW = pieceCuts[hardestPiece].second;

  CompressionParams unifiedParams =
      findOptimalParams(ffmpegPath, m_inputPath, hardestCropW, height,
                        hardestLeft, targetFps, TARGET_SIZE_MB);

  emit statusMessage(QString("使用统一压缩: %1, 缩放%2%, 帧率%3fps")
                         .arg(unifiedParams.name)
                         .arg(unifiedParams.scale * 100, 0, 'f', 0)
                         .arg(unifiedParams.fps, 0, 'f', 1));

  // ========== 第三步：用统一参数生成所有块 ==========
  QStringList outputFiles;
  QStringList warnings;

  for (int piece = 0; piece < 5; ++piece) {
    int left = pieceCuts[piece].first;
    int cropW = pieceCuts[piece].second;

    emit statusMessage(QString("正在生成第 %1/5 块...").arg(piece + 1));

    QString outputPath =
        QString("%1/steam_gif_%2.gif").arg(m_outputDir).arg(piece + 1);

    bool success =
        compressWithUnifiedParams(ffmpegPath, m_inputPath, outputPath, cropW,
                                  height, left, unifiedParams, piece + 1);

    if (!success) {
      emit error(QString("第 %1 块生成失败").arg(piece + 1));
      return;
    }

    QFileInfo info(outputPath);
    double sizeMB = info.size() / (1024.0 * 1024.0);

    if (sizeMB > TARGET_SIZE_MB) {
      warnings.append(
          QString("第%1块: %2 MB").arg(piece + 1).arg(sizeMB, 0, 'f', 2));
    }

    outputFiles.append(outputPath);
    emit progressUpdated(50 + (piece + 1) * 10);
  }

  // 发出警告
  if (!warnings.isEmpty()) {
    QString warningMsg = QString("⚠️ 以下 GIF 文件超过 %1 MB "
                                 "限制：\n%"
                                 "2\n\n注意：为确保拼接对齐，所有块使用相同压缩"
                                 "参数。\n建议减少原始 GIF 的帧数或时长。")
                             .arg(TARGET_SIZE_MB)
                             .arg(warnings.join("\n"));
    emit sizeWarning(warningMsg);
  }

  verifyAlignment(outputFiles);

  emit statusMessage("全部完成！");
  emit finished(outputFiles);
}

void GifProcessor::fixGifForSteam(const QString &gifPath) {
  QFile file(gifPath);
  if (!file.open(QIODevice::ReadWrite)) {
    qWarning() << "无法打开文件进行 Steam 修复:" << gifPath;
    return;
  }

  // 获取文件大小
  qint64 fileSize = file.size();
  if (fileSize < 2) {
    file.close();
    return;
  }

  // 定位到最后一个字节
  file.seek(fileSize - 1);

  // 读取最后一个字节
  char lastByte;
  file.read(&lastByte, 1);

  // 如果最后一个字节是 0x3B (GIF 结束标记)，改为 0x21
  if ((unsigned char)lastByte == 0x3B) {
    file.seek(fileSize - 1);
    char newByte = 0x21;
    file.write(&newByte, 1);
    qDebug() << "Steam 修复: 将" << gifPath << "末尾字节从 0x3B 改为 0x21";
  } else {
    qDebug() << "Steam 修复: 文件" << gifPath << "末尾字节不是 0x3B，当前值:"
             << QString::number((unsigned char)lastByte, 16);
  }

  file.close();
}