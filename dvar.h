#pragma once

enum DvarType
{
	DVAR_VALUE_BOOL,
	DVAR_VALUE_INT,
	DVAR_VALUE_STRING
};

union dvar_value_u
{
	bool boolean;
	int integer;
	float decimal;
	QString* string;
};

struct dvar_s
{
	const char* name;
	const char* description;
	DvarType type;
	dvar_value_u defaultValue;
	int maxValue;
	bool isCmd;
};

class Dvar
{
private:
	dvar_s dvar;
	dvar_value_u* mValue;

public:
	Dvar();
	Dvar(dvar_s, QTreeWidget*);
	~Dvar();

	static QString setDvarSetting(dvar_s, QCheckBox*);
	static QString setDvarSetting(dvar_s, QSpinBox*);
	static QString setDvarSetting(dvar_s, QLineEdit*);

	static dvar_s findDvar(QString, QTreeWidget*, dvar_s*, int);
};
