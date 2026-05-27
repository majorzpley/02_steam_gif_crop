/*
 * @Author: majorzpley majorzpley@zohomail.cn
 * @Date: 2026-05-26 23:57:43
 * @LastEditors: majorzpley majorzpley@zohomail.cn
 * @LastEditTime: 2026-05-27 02:54:13
 * @FilePath: \01_steam_gif_cutter\main.cpp
 * @Description:
 * 不用客气，这是你应该谢的!
 * Copyright (c) 2026 by ${git_name_email}, All Rights Reserved.
 */
#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QImageWriter>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  // debug 检查支持的图像格式，调试信息
  //    QList<QByteArray> formats = QImageWriter::supportedImageFormats();
  //    qDebug() << "支持的写入格式:";
  //    for (const QByteArray &format : formats) {
  //      qDebug() << "  " << format;
  //    }

  app.setApplicationName("Steam GIF Cutter");
  app.setApplicationVersion("1.0");

  MainWindow window;
  window.show();

  return app.exec();
}