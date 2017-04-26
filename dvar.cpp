#include "stdafx.h"

Dvar::Dvar(dvar_s _dvar, QTreeWidget *_dvarTree) : dvar(_dvar)
{
	QString dvarSetting = QString("dvar_%1").arg(dvar.name);
	QSettings settings;

	QTreeWidgetItem* Item = new QTreeWidgetItem(_dvarTree, QStringList() << dvar.name);
	Item->setText(0, dvar.name);
	Item->setToolTip(0, dvar.description);

	QCheckBox* checkBox;
	QSpinBox* spinBox;
	QLineEdit* textBox;

	switch(this->dvar.type)
	{
	case DVAR_VALUE_BOOL:
		checkBox = new QCheckBox();
		checkBox->setChecked(settings.value(dvarSetting, false).toBool());
		checkBox->setToolTip("Boolean value, check to enable or uncheck to disable.");
		_dvarTree->setItemWidget(Item, 1, checkBox);
		break;
	case DVAR_VALUE_INT:
		spinBox = new QSpinBox();
		spinBox->setValue(settings.value(dvarSetting, 0).toInt());
		spinBox->setToolTip("Integer value, min to max any number.");
		spinBox->setMaximum(dvar.maxValue);
		spinBox->setMinimum(dvar.minValue);
		_dvarTree->setItemWidget(Item, 1, spinBox);
		break;
	case DVAR_VALUE_STRING:
		textBox = new QLineEdit();
		textBox->setText(settings.value(dvarSetting, "").toString());
		textBox->setToolTip(QString("String value, leave this blank for it to not be used."));
		textBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
		_dvarTree->setItemWidget(Item, 1, textBox);
		break;
	}
}

Dvar::~Dvar()
{
}

dvar_s Dvar::findDvar(QString _dvarName, QTreeWidget* DvarTree, dvar_s* dvars, int DvarSize)
{
	dvar_s _dvar;
	for(int DvarIdx = 0; DvarIdx < DvarSize; DvarIdx++)
	{
		_dvar = Dvar(dvars[DvarIdx], DvarTree).dvar;
		if(_dvar.name == _dvarName)
			return _dvar;
	}
	return _dvar;
}

QString Dvar::setDvarSetting(dvar_s _dvar, QCheckBox* _checkBox)
{
	QSettings Settings;
	Settings.setValue(QString("dvar_%1").arg(_dvar.name), _checkBox->isChecked());

	return Settings.value(QString("dvar_%1").arg(_dvar.name)).toString() == "true" ? "1" : "0"; // another way to do this?
}

QString Dvar::setDvarSetting(dvar_s _dvar, QSpinBox* _spinBox)
{
	QSettings Settings;
	Settings.setValue(QString("dvar_%1").arg(_dvar.name), _spinBox->value());

	return Settings.value(QString("dvar_%1").arg(_dvar.name)).toString();
}

QString Dvar::setDvarSetting(dvar_s _dvar, QLineEdit* _lineEdit)
{
	QSettings Settings;
	Settings.setValue(QString("dvar_%1").arg(_dvar.name), _lineEdit->text());

	return Settings.value(QString("dvar_%1").arg(_dvar.name)).toString();
}
