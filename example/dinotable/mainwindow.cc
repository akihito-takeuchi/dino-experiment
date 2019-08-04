// Copyright (c) 2019 Akihito Takeuchi
// Distributed under the MIT License : http://opensource.org/licenses/MIT

#include "mainwindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QTableView>
#include <QHeaderView>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>

#include "dino/qt/dobjecttablemodel.h"
#include "dinotable.h"

class RowNameInputDialog : public QDialog {
  Q_OBJECT
 public:
  RowNameInputDialog(QWidget* parent, const QStringList& existing_names)
      : QDialog(parent), existing_names_(existing_names) {
    row_name_input_ = new QLineEdit;
    buttons_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto layout = new QFormLayout;
    layout->addRow("Input row name", row_name_input_);
    layout->addRow(buttons_);
    setLayout(layout);
    UpdateState();
    connect(row_name_input_, &QLineEdit::textChanged,
            this, &RowNameInputDialog::UpdateState);
    connect(buttons_->button(QDialogButtonBox::Ok), &QPushButton::clicked,
            this, &QDialog::accept);
    connect(buttons_->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);
  }
  QString Name() const {
    return row_name_input_->text();
  }

 private slots:
  void UpdateState() {
    auto name = row_name_input_->text();
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(
        !existing_names_.contains(name)
        && dino::core::DObjPath::IsValidName(name.toStdString()));
  }
 private:
  QStringList existing_names_;
  QLineEdit* row_name_input_;
  QDialogButtonBox* buttons_;
};

MainWindow::MainWindow(const dino::core::DObjectSp& root_object,
                       QWidget* parent)
    : QMainWindow(parent), root_object_(root_object) {
  CreateActions();
  CreateMenus();
  CreateToolBars();
  CreateWidgets();
  CreateConnections();
  InitTable();
}

MainWindow::~MainWindow() = default;

void MainWindow::CreateActions() {
  save_action_ = new QAction(
      qApp->style()->standardIcon(QStyle::SP_DriveFDIcon),
      "Save", this);
  close_action_ = new QAction(
      qApp->style()->standardIcon(QStyle::SP_DirClosedIcon),
      "Close", this);
  add_row_action_ = new QAction("Add row", this);
  add_column_action_ = new QAction("Add column", this);
  insert_column_action_ = new QAction("Insert column", this);
  delete_row_action_ = new QAction("Delete row", this);
}

void MainWindow::CreateMenus() {
  auto file_menu = menuBar()->addMenu("&File");
  file_menu->addAction(save_action_);
  file_menu->addAction(close_action_);
}

void MainWindow::CreateToolBars() {
  auto edit_tool_bar = addToolBar("Edit");
  edit_tool_bar->addAction(add_row_action_);
  edit_tool_bar->addAction(add_column_action_);
}

void MainWindow::CreateWidgets() {
  table_ = new QTableView;
  table_->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  table_->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  setCentralWidget(table_);
}

void MainWindow::CreateConnections() {
  connect(close_action_, &QAction::triggered,
          this, &MainWindow::close);
  connect(table_->horizontalHeader(), &QWidget::customContextMenuRequested,
          this, &MainWindow::ShowContextMenuForHorizontalHeader);
  connect(table_->verticalHeader(), &QWidget::customContextMenuRequested,
          this, &MainWindow::ShowContextMenuForVerticalHeader);
  connect(add_row_action_, &QAction::triggered,
          this, &MainWindow::AddRow);
}

void MainWindow::InitTable() {
  model_ = new dino::qt::DObjectTableModel(root_object_);
  table_->setModel(model_);
  dino::qt::ColumnInfo id_col(
      "ID", dino::qt::SourceTypeConst::kName, "",
      [](auto obj, int role) {
        if (role == Qt::DisplayRole)
          return QVariant(QString::fromStdString(obj->Name()));
        else
          return QVariant();
      },
      [](auto) {
        return Qt::ItemIsSelectable;
      });
  model_->InsertColumns(0, {id_col});
}

void MainWindow::ShowContextMenuForHorizontalHeader(const QPoint& pos) {
  auto sender_widget = qobject_cast<QWidget*>(sender());
  QMenu menu;
  menu.addAction(insert_column_action_);
  menu.exec(sender_widget->mapToGlobal(pos));
}

void MainWindow::ShowContextMenuForVerticalHeader(const QPoint& pos) {
  QMenu menu;
  menu.addAction(delete_row_action_);
  menu.exec(pos);
}

void MainWindow::AddRow() {
  QStringList existing_ids;
  for (auto& child_info : root_object_->Children())
    existing_ids.append(QString::fromStdString(child_info.Name()));
  RowNameInputDialog dlg(this, existing_ids);
  if (dlg.exec() == QDialog::Rejected)
    return;
  root_object_->CreateChild(dlg.Name().toStdString(),
                            kDinoTableRowType);
}

#include "mainwindow.moc"

