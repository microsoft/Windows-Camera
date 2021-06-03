// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;

namespace VirtualCameraManager_App
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    sealed partial class App : Application
    {
        /// <summary>
        /// Internal class to cache the parameters with which a previous virtual camera was created in the app upon suspending.
        /// This allows then the app to take control of previous virtual cameras it knows how it created upon launching.
        /// </summary>
        internal class VCamInfo
        {
            public string FriendlyName;
            public int VCamType;
            public string SymLink;
            public string WrappedCameraSymLink;
        }

        // members to handle known virtual cameras
        internal static List<VCamInfo> m_virtualCameraInfoList = new List<VCamInfo>();
        internal SemaphoreSlim m_lock = new SemaphoreSlim(1);
        private const string CONFIG_FILE_NAME = "config.txt";


        /// <summary>
        /// Initializes the singleton application object.  This is the first line of authored code
        /// executed, and as such is the logical equivalent of main() or WinMain().
        /// </summary>
        public App()
        {
            this.InitializeComponent();
            this.Suspending += OnSuspending;
        }

        /// <summary>
        /// Invoked when the application is launched normally by the end user.  Other entry points
        /// will be used such as when the application is launched to open a specific file.
        /// </summary>
        /// <param name="e">Details about the launch request and process.</param>
        protected override async void OnLaunched(LaunchActivatedEventArgs e)
        {
            // Load virtual camera configurations (if any)
            await LoadConfigFileAsync();

            Frame rootFrame = Window.Current.Content as Frame;

            // Do not repeat app initialization when the Window already has content,
            // just ensure that the window is active
            if (rootFrame == null)
            {
                // Create a Frame to act as the navigation context and navigate to the first page
                rootFrame = new Frame();

                rootFrame.NavigationFailed += OnNavigationFailed;

                if (e.PreviousExecutionState == ApplicationExecutionState.Terminated)
                {
                    //TODO: Load state from previously suspended application
                }

                // Place the frame in the current Window
                Window.Current.Content = rootFrame;
            }

            if (e.PrelaunchActivated == false)
            {
                if (rootFrame.Content == null)
                {
                    // When the navigation stack isn't restored navigate to the first page,
                    // configuring the new page by passing required information as a navigation
                    // parameter
                    rootFrame.Navigate(typeof(MainPage), e.Arguments);
                }
                // Ensure the current window is active
                Window.Current.Activate();
            }
        }

        /// <summary>
        /// Invoked when Navigation to a certain page fails
        /// </summary>
        /// <param name="sender">The Frame which failed navigation</param>
        /// <param name="e">Details about the navigation failure</param>
        void OnNavigationFailed(object sender, NavigationFailedEventArgs e)
        {
            throw new Exception("Failed to load Page " + e.SourcePageType.FullName);
        }

        /// <summary>
        /// Invoked when application execution is being suspended.  Application state is saved
        /// without knowing whether the application will be terminated or resumed with the contents
        /// of memory still intact.
        /// </summary>
        /// <param name="sender">The source of the suspend request.</param>
        /// <param name="e">Details about the suspend request.</param>
        private async void OnSuspending(object sender, SuspendingEventArgs e)
        {
            // Save virtual camera configurations (if any)
            await WriteToConfigFileAsync();

            var deferral = e.SuspendingOperation.GetDeferral();
            //TODO: Save application state and stop any background activity
            deferral.Complete();
        }

        /// <summary>
        /// Method to search and load if found a local config file containing 
        /// the information about previously registered virtual cameras with this app.
        /// </summary>
        /// <returns></returns>
        private async Task LoadConfigFileAsync()
        {
            m_lock.Wait();
            try
            {
                var item = await ApplicationData.Current.LocalFolder.TryGetItemAsync(CONFIG_FILE_NAME);
                if (item != null)
                {
                    var configFile = item as StorageFile;
                    var configurations = await Windows.Storage.FileIO.ReadLinesAsync(configFile);
                    foreach (var line in configurations)
                    {
                        var configItems = line.Split("|");
                        if (configItems.Count() == 4)
                        {
                            m_virtualCameraInfoList.Add(new VCamInfo()
                            {
                                FriendlyName = configItems[0],
                                VCamType = Int32.Parse(configItems[1]),
                                SymLink = configItems[2],
                                WrappedCameraSymLink = configItems[3]
                            });
                        }
                    }
                }
            }
            finally
            {
                m_lock.Release();
            }
        }

       
        /// <summary>
        /// Method to write to a config files the set of Virtual Cameras created with the app
        /// </summary>
        /// <returns></returns>
        private async Task WriteToConfigFileAsync()
        {
            m_lock.Wait();
            try
            {
                var item = await ApplicationData.Current.LocalFolder.TryGetItemAsync(CONFIG_FILE_NAME);
                StorageFile configFile = null;
                if (item != null)
                {
                    configFile = item as StorageFile;

                }
                else
                {
                    configFile = await ApplicationData.Current.LocalFolder.CreateFileAsync(CONFIG_FILE_NAME);
                }

                List<string> vCamInfoToWrite = new List<string>();
                foreach (var vcam in MainPage.m_virtualCameraControls)
                {
                    if (vcam.VirtualCameraProxyInst.IsKnownInstance)
                    {
                        vCamInfoToWrite.Add(vcam.VirtualCameraProxyInst.FriendlyName
                            + "|" + (int)vcam.VirtualCameraProxyInst.VirtualCameraKind
                            + "|" + vcam.VirtualCameraProxyInst.SymbolicLink
                            + "|" + vcam.VirtualCameraProxyInst.WrappedCameraSymbolicLink);
                    }
                }

                await FileIO.WriteLinesAsync(configFile, vCamInfoToWrite);
            }
            finally
            {
                m_lock.Release();
            }
        }
    }
}
