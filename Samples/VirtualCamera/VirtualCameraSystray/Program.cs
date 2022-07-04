// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Threading;
using System.Windows.Forms;

namespace VirtualCameraSystray
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Mutex mutex = null;
            if (!Mutex.TryOpenExisting("VirtualCameraSystrayMutex", out mutex))
            {
                mutex = new Mutex(false, "VirtualCameraSystrayMutex");
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new SystrayApplicationContext());
                mutex.Close();
            }
        }
    }
}
