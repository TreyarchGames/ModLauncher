#pragma once

enum DvarType
{
	DVAR_VALUE_BOOL,
	DVAR_VALUE_INT,
	DVAR_VALUE_STRING
};

struct dvar_s
{
	const char* name;
	const char* description;
	DvarType type;
	int minValue;
	int maxValue;
	bool isCmd;
};

class Dvar
{
private:
	dvar_s dvar;

public:
	Dvar();
	Dvar(dvar_s, QTreeWidget*);
	~Dvar();

	static QString setDvarSetting(dvar_s, QCheckBox*);
	static QString setDvarSetting(dvar_s, QSpinBox*);
	static QString setDvarSetting(dvar_s, QLineEdit*);

	static dvar_s findDvar(QString, QTreeWidget*, dvar_s*, int);
};
