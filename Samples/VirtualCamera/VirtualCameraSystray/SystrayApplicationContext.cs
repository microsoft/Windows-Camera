// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Diagnostics;
using System.IO.Pipes;
using System.Windows.Forms;
using WinSys = Windows.System;

namespace VirtualCameraSystray
{
    class SystrayApplicationContext : ApplicationContext
    {
        // members
        private NotifyIcon notifyIcon = null;
        private NamedPipeServerStream m_pipeServerForStart = null;
        private const string m_pipeNameForStart = @"\\.\pipe\ContosoVCamPipeStart";

        // methods

        /// <summary>
        /// Constructor; populate SystemTray icons and actions, start a named pipe to listen for when the virtual camera starts streaming.
        /// </summary>
        public SystrayApplicationContext()
        {
            var items = new MenuItem[]
            {
                new MenuItem("📷 App", new EventHandler(OpenApp)),
                new MenuItem("❌ Close", new EventHandler(Exit))
            };

            notifyIcon = new NotifyIcon();
            notifyIcon.DoubleClick += new EventHandler(OpenApp);
            notifyIcon.Icon = VirtualCameraSystray.Properties.Resources.Icon1;
            notifyIcon.ContextMenu = new ContextMenu(items);
            notifyIcon.Text = "Contoso virtual camera";

            CreateNamedPipeForStart();

            notifyIcon.Visible = true;
        }

        /// <summary>
        /// Open the main control app
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void OpenApp(object sender, EventArgs e)
        {
            var uri = new Uri("contoso-vcam:");
            var success = await WinSys.Launcher.LaunchUriAsync(uri);
        }

        /// <summary>
        /// Exit the system tray process, closing the app for good
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void Exit(object sender, EventArgs e)
        {
            var uri = new Uri("contoso-vcam:exit");
            var success = await WinSys.Launcher.LaunchUriAsync(uri);
            Application.Exit();
        }


        /// <summary>
        /// Create a named pipe and wait fo connection from the virtual camera to signal the start of the main control app
        /// </summary>
        private void CreateNamedPipeForStart()
        {
            if (m_pipeServerForStart != null)
            {
                m_pipeServerForStart.Close();
                m_pipeServerForStart.Dispose();
                m_pipeServerForStart = null;
            }
            PipeSecurity ps = new PipeSecurity();
            System.Security.Principal.SecurityIdentifier sid = new System.Security.Principal.SecurityIdentifier(System.Security.Principal.WellKnownSidType.WorldSid, null);
            PipeAccessRule par = new PipeAccessRule(sid, PipeAccessRights.ReadWrite, System.Security.AccessControl.AccessControlType.Allow);
            ps.AddAccessRule(par);
            m_pipeServerForStart = new NamedPipeServerStream(m_pipeNameForStart, PipeDirection.InOut, 1, PipeTransmissionMode.Byte, PipeOptions.Asynchronous, 255, 255, ps);
            m_pipeServerForStart.BeginWaitForConnection(
                new AsyncCallback(WaitForConnectionCallBackForStart),
                m_pipeServerForStart);
        }

        /// <summary>
        /// When we connect to the named pipe, start the main control app
        /// </summary>
        /// <param name="result"></param>
        private async void WaitForConnectionCallBackForStart(IAsyncResult result)
        {
            try
            {
                // Get the pipe
                NamedPipeServerStream pipeServer = (NamedPipeServerStream)result.AsyncState;

                // End waiting for the connection
                pipeServer.EndWaitForConnection(result);

                // Launch main app
                OpenApp(null, null);

            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Exception caught in WaitForConnectionCallBackForStart(): {ex.Message}");
                return;
            }

            // rinse-repeat
            CreateNamedPipeForStart();
        }
    }
}
