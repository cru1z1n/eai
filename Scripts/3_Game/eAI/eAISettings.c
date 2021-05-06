class eAISettings : JsonApiStruct
{
	static ref ScriptInvoker ON_UPDATE = new ScriptInvoker();

	private static const string PATH = "$profile:eAI/eAISettings.json";

	private static ref eAISettings m_Instance = new eAISettings();

	private LogLevel m_LogLevel = LogLevel.WARNING;
	private float m_Accuracy = 0.5;

	void SetLogLevel(LogLevel logLevel)
	{
		m_LogLevel = logLevel;
	}

	static LogLevel GetLogLevel()
	{
		return m_Instance.m_LogLevel;
	}

	void SetAccuracy(float accuracy)
	{
		m_Accuracy = accuracy;
	}

	static float GetAccuracy()
	{
		return m_Instance.m_Accuracy;
	}

	override void OnInteger(string name, int value)
	{
		if (name == "LogLevel")
		{
			SetLogLevel(value);
			return;
		}
	}

	override void OnFloat(string name, float value)
	{
		if (name == "Accuracy")
		{
			SetAccuracy(value);
			return;
		}
	}

	override void OnPack()
	{
		StoreInteger("LogLevel", m_LogLevel);
		StoreFloat("Accuracy", m_Accuracy);
	}

	override void OnSuccess(int errorCode)
	{
		ON_UPDATE.Invoke();
	}

	override void OnError(int errorCode)
	{
		Print(errorCode);
		ON_UPDATE.Invoke();
	}

	static void Init()
	{
		if (!GetJsonApi())
		{
			CreateJsonApi();
		}

		if (!FileExist(PATH))
		{
			m_Instance.Save();
			ON_UPDATE.Invoke();
			return;
		}

		m_Instance.Load();
	}

	static eAISettings Get()
	{
		return m_Instance;
	}

	private void eAISettings() {}

	void Load()
	{
		FileHandle file_handle = OpenFile(PATH, FileMode.READ);
		string content;
		string line_content;
		while (FGets(file_handle, line_content) >= 0)
		{
			content += line_content;
		}

		CloseFile(file_handle);

		ExpandFromRAW(content);
	}

	void Save()
	{
		InstantPack();
		SaveToFile(PATH);
	}
};