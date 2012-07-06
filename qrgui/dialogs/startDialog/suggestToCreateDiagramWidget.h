#pragma once
#include <QtGui/QWidget>
#include <QtGui/QPushButton>
#include <QtGui/QListWidget>

namespace qReal {

class MainWindow;

}

namespace qReal {

class SuggestToCreateDiagramWidget : public QWidget
{
	Q_OBJECT

public:
	explicit SuggestToCreateDiagramWidget(MainWindow *mainWindow, QDialog *parent = 0);

signals:
	void diagramCreated();

private:
	MainWindow *mMainWindow;
	QListWidget *mDiagramsListWidget;
};

}
