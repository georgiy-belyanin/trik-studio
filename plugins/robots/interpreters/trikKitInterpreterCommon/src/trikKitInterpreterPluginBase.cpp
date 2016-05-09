/* Copyright 2013-2016 CyberTech Labs Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include "trikKitInterpreterCommon/trikKitInterpreterPluginBase.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>

#include <twoDModel/engine/twoDModelEngineFacade.h>
#include <qrkernel/settingsManager.h>
#include <qrkernel/settingsListener.h>

#include <qrgui/textEditor/qscintillaTextEdit.h>

using namespace trik;
using namespace qReal;

const Id robotDiagramType = Id("RobotsMetamodel", "RobotsDiagram", "RobotsDiagramNode");
const Id subprogramDiagramType = Id("RobotsMetamodel", "RobotsDiagram", "SubprogramDiagram");

TrikKitInterpreterPluginBase::TrikKitInterpreterPluginBase() :
	mStart(tr("Start QTS"), nullptr), mStop(tr("Stop QTS"), nullptr)
{
}

TrikKitInterpreterPluginBase::~TrikKitInterpreterPluginBase()
{
	if (mOwnsAdditionalPreferences) {
		delete mAdditionalPreferences;
	}

	if (mOwnsBlocksFactory) {
		delete mBlocksFactory;
	}
}

void TrikKitInterpreterPluginBase::initKitInterpreterPluginBase
		(robotModel::TrikRobotModelBase * const realRobotModel
		, robotModel::twoD::TrikTwoDRobotModel * const twoDRobotModel
		, blocks::TrikBlocksFactoryBase * const blocksFactory
		)
{
	mRealRobotModel.reset(realRobotModel);
	mTwoDRobotModel.reset(twoDRobotModel);
	mBlocksFactory = blocksFactory;

	const auto modelEngine = new twoDModel::engine::TwoDModelEngineFacade(*mTwoDRobotModel);

	mTwoDRobotModel->setEngine(modelEngine->engine());
	mTwoDModel.reset(modelEngine);

	connectDevicesConfigurationProvider(devicesConfigurationProvider()); // ... =(

	mAdditionalPreferences = new TrikAdditionalPreferences({ mRealRobotModel->name() });

	mQtsInterpreter.reset(new TrikQtsInterpreter(mTwoDRobotModel));
}

TrikQtsInterpreter * TrikKitInterpreterPluginBase::qtsInterpreter() const
{
	return mQtsInterpreter.data();
}

void TrikKitInterpreterPluginBase::init(const kitBase::KitPluginConfigurator &configurer)
{
	connect(&configurer.eventsForKitPlugin()
			, &kitBase::EventsForKitPluginInterface::robotModelChanged
			, [this](const QString &modelName) { mCurrentlySelectedModelName = modelName; });

	qReal::gui::MainWindowInterpretersInterface &interpretersInterface
			= configurer.qRealConfigurator().mainWindowInterpretersInterface();

	mTwoDModel->init(configurer.eventsForKitPlugin()
			, configurer.qRealConfigurator().systemEvents()
			, configurer.qRealConfigurator().logicalModelApi()
			, configurer.qRealConfigurator().controller()
			, interpretersInterface
			, configurer.qRealConfigurator().mainWindowDockInterface()
			, configurer.qRealConfigurator().projectManager()
			, configurer.interpreterControl());

	mRealRobotModel->setErrorReporter(*interpretersInterface.errorReporter());
	mTwoDRobotModel->setErrorReporter(*interpretersInterface.errorReporter());

	mQtsInterpreter->setErrorReporter(*interpretersInterface.errorReporter());

	mMainWindow = &configurer.qRealConfigurator().mainWindowInterpretersInterface();

	mSystemEvents = &configurer.qRealConfigurator().systemEvents();

	/// @todo: refactor?
	mStart.setObjectName("runQts");
	mStart.setText(tr("Run program"));
	mStart.setIcon(QIcon(":/trik/qts/images/run.png"));

	mStop.setObjectName("stopQts");
	mStop.setText(tr("Stop robot"));
	mStop.setIcon(QIcon(":/trik/qts/images/stop.png"));

	mStop.setVisible(false);
	mStart.setVisible(false);

	connect(&configurer.robotModelManager()
			, &kitBase::robotModel::RobotModelManagerInterface::robotModelChanged
			, [this](kitBase::robotModel::RobotModelInterface &model){
		mIsModelSelected = robotModels().contains(&model);
//		kitBase::robotModel::RobotModelInterface * const ourModel = robotModels()[0];
//		for (const ActionInfo &action : customActions()) {
//			if (action.isAction()) {
//				action.action()->setVisible(robotModels().contains(&model));
//			} else {
//				action.menu()->setVisible(robotModels().contains(&model));
//			}
//		}
	});

	connect(&mStart, &QAction::triggered, this, &trik::TrikKitInterpreterPluginBase::testStart);
	connect(&mStop, &QAction::triggered, this, &trik::TrikKitInterpreterPluginBase::testStop);
	// refactor?
	connect(this
			, &TrikKitInterpreterPluginBase::started
			, &configurer.eventsForKitPlugin()
			, &kitBase::EventsForKitPluginInterface::interpretationStarted
			);

	connect(this
			, &TrikKitInterpreterPluginBase::stopped
			, &configurer.eventsForKitPlugin()
			, &kitBase::EventsForKitPluginInterface::interpretationStopped
			);

	QObject::connect(
				&configurer.eventsForKitPlugin()
				, &kitBase::EventsForKitPluginInterface::interpretationStarted
				, [this](){ /// @todo
		const bool isQtsInt = mQtsInterpreter->isRunning();
		mStart.setEnabled(isQtsInt);
		mStop.setEnabled(isQtsInt);
	}
	);

	QObject::connect(
				&configurer.eventsForKitPlugin()
				, &kitBase::EventsForKitPluginInterface::interpretationStopped
				, [this](qReal::interpretation::StopReason reason){ /// @todo
		Q_UNUSED(reason);
		mStart.setEnabled(true);
		mStop.setEnabled(true);
	}
	);

	connect(mSystemEvents
			, &qReal::SystemEvents::activeTabChanged
			, this
			, &TrikKitInterpreterPluginBase::onTabChanged);

	connect(mAdditionalPreferences, &TrikAdditionalPreferences::settingsChanged
			, mRealRobotModel.data(), &robotModel::TrikRobotModelBase::rereadSettings);

	connect(mAdditionalPreferences, &TrikAdditionalPreferences::settingsChanged
			, mTwoDRobotModel.data(), &robotModel::twoD::TrikTwoDRobotModel::rereadSettings);
}

QList<kitBase::robotModel::RobotModelInterface *> TrikKitInterpreterPluginBase::robotModels()
{
	return {mRealRobotModel.data(), mTwoDRobotModel.data()};
}

kitBase::blocksBase::BlocksFactoryInterface *TrikKitInterpreterPluginBase::blocksFactoryFor(
		const kitBase::robotModel::RobotModelInterface *model)
{
	Q_UNUSED(model);

	mOwnsBlocksFactory = false;
	return mBlocksFactory;
}

kitBase::robotModel::RobotModelInterface *TrikKitInterpreterPluginBase::defaultRobotModel()
{
	return mTwoDRobotModel.data();
}

QList<kitBase::AdditionalPreferences *> TrikKitInterpreterPluginBase::settingsWidgets()
{
	mOwnsAdditionalPreferences = false;
	return {mAdditionalPreferences};
}

QWidget *TrikKitInterpreterPluginBase::quickPreferencesFor(const kitBase::robotModel::RobotModelInterface &model)
{
	return model.name().toLower().contains("twod")
			? nullptr
			: produceIpAddressConfigurer();
}

QList<qReal::ActionInfo> TrikKitInterpreterPluginBase::customActions()
{
	return { qReal::ActionInfo(&mStart, "interpreters", "tools"), qReal::ActionInfo(&mStop, "interpreters", "tools") };
}

QList<HotKeyActionInfo> TrikKitInterpreterPluginBase::hotKeyActions()
{
	return {};
}

QString TrikKitInterpreterPluginBase::defaultSettingsFile() const
{
	return ":/trikDefaultSettings.ini";
}

QIcon TrikKitInterpreterPluginBase::iconForFastSelector(
		const kitBase::robotModel::RobotModelInterface &robotModel) const
{
	return &robotModel == mRealRobotModel.data()
			? QIcon(":/icons/switch-real-trik.svg")
			: &robotModel == mTwoDRobotModel.data()
					? QIcon(":/icons/switch-2d.svg")
					: QIcon();
}

kitBase::DevicesConfigurationProvider *TrikKitInterpreterPluginBase::devicesConfigurationProvider()
{
	return &mTwoDModel->devicesConfigurationProvider();
}

QWidget *TrikKitInterpreterPluginBase::produceIpAddressConfigurer()
{
	QLineEdit * const quickPreferences = new QLineEdit;
	quickPreferences->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	quickPreferences->setPlaceholderText(tr("Enter robot`s IP-address here..."));
	const auto updateQuickPreferences = [quickPreferences]() {
		const QString ip = qReal::SettingsManager::value("TrikTcpServer").toString();
		if (quickPreferences->text() != ip) {
			quickPreferences->setText(ip);
		}
	};

	updateQuickPreferences();
	connect(mAdditionalPreferences, &TrikAdditionalPreferences::settingsChanged, updateQuickPreferences);
	qReal::SettingsListener::listen("TrikTcpServer", updateQuickPreferences, this);
	connect(quickPreferences, &QLineEdit::textChanged, [](const QString &text) {
		qReal::SettingsManager::setValue("TrikTcpServer", text);
	});

	connect(this, &QObject::destroyed, [quickPreferences]() { delete quickPreferences; });
	return quickPreferences;
}

void TrikKitInterpreterPluginBase::testStart()
{
	mStop.setVisible(true);
	mStart.setVisible(false);
	/// todo: bad


	auto texttab = dynamic_cast<qReal::text::QScintillaTextEdit *>(mMainWindow->currentTab());

	if (texttab) {

		auto model = mTwoDRobotModel;
		model->stopRobot(); // testStop?
		const QString modelName = model->robotId();

		for (const kitBase::robotModel::PortInfo &port : model->configurablePorts()) {
			const kitBase::robotModel::DeviceInfo deviceInfo = currentConfiguration(modelName, port);
			model->configureDevice(port, deviceInfo);
		}

		model->applyConfiguration();

		qtsInterpreter()->init();

		qtsInterpreter()->setRunning(true);
		emit started();
		qtsInterpreter()->interpretScript(texttab->text());
	}
}

void TrikKitInterpreterPluginBase::testStop()
{
	mStop.setVisible(false);
	mStart.setVisible(true);

	qtsInterpreter()->abort();
	mTwoDRobotModel->stopRobot();
	emit stopped(qReal::interpretation::StopReason::userStop);
}

void TrikKitInterpreterPluginBase::onTabChanged(const TabInfo &info)
{
	const bool isCodeTab = info.type() == qReal::TabInfo::TabType::code;
	/// @todo: hack!
	mStart.setVisible(mIsModelSelected && isCodeTab);
	mStop.setVisible(mIsModelSelected && isCodeTab);
}
