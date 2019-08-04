// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#pragma once

#include <QMainWindow>
#include <dino/core/dobject.h>

class QAction;
class QTableView;

namespace dino {

namespace qt {

class DObjectTableModel;

}  // namespace qt

}  // namespace dino

class MainWindow : public QMainWindow {
 public:
  MainWindow(const dino::core::DObjectSp& root_object,
             QWidget* parent = nullptr);
  ~MainWindow();

 private slots:
  void ShowContextMenuForHorizontalHeader(const QPoint& pos);
  void ShowContextMenuForVerticalHeader(const QPoint& pos);
  void AddRow();

 private:
  void CreateActions();
  void CreateMenus();
  void CreateToolBars();
  void CreateWidgets();
  void CreateConnections();
  void InitTable();

  QAction* save_action_;
  QAction* close_action_;
  QAction* add_row_action_;
  QAction* add_column_action_;
  QAction* insert_column_action_;
  QAction* delete_row_action_;

  QTableView* table_;
  dino::qt::DObjectTableModel* model_;
  dino::core::DObjectSp root_object_;
};
