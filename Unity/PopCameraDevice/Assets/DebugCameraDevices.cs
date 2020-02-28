using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class DebugCameraDevices : MonoBehaviour {

	void OnEnable ()
	{
		var DeviceNames = PopCameraDevice.EnumCameraDevices();
		foreach(var Device in DeviceNames)
		{
			var FormatString = string.Join(",",Device.Formats);
			Debug.Log(Device.Serial + " formats: " + FormatString);
		}
	}
}
