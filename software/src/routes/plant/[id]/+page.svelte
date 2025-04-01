<script lang="ts">
	import { browser } from '$app/environment';
	import { onMount, onDestroy } from 'svelte';
	import { fade, fly } from 'svelte/transition';
	import {
		Loader2,
		Droplet,
		Power,
		Settings,
		AlertTriangle,
		RefreshCw,
		Clock
	} from 'lucide-svelte';

	// Type definitions
	type MoistureDataPoint = {
		timestamp: Date;
		moisture: number;
	};

	type DeviceMode = 'auto' | 'manual';
	type PumpState = 'on' | 'off';
	type ConnectionStatus = 'connected' | 'disconnected';
	type CommandType = 'pump' | 'mode' | 'threshold';

	interface DeviceStatusData {
		deviceId?: string;
		moisture?: number;
		timestamp?: string | number;
		threshold?: number;
		pumpState?: PumpState;
		mode?: DeviceMode;
	}

	interface StatusResponse {
		status: ConnectionStatus;
		latestMessage?: {
			payload: DeviceStatusData;
		};
	}

	interface CommandRequest {
		command: CommandType;
		state?: PumpState | DeviceMode | null;
		value?: number | null;
	}

	// State variables
	let deviceId: string = '';
	let moisture: number = 0;
	let mode: DeviceMode = 'auto';
	let threshold: number = 30;
	let pollingInterval: ReturnType<typeof setInterval> | undefined;
	let timestamp: string | number | null = null;
	let connecting: boolean = true;
	let pumpState: PumpState = 'off';
	let lastUpdated: Date | null = null;
	let dataReceived: boolean = false;
	let connectionError: string | null = null;
	let connectionStatus: ConnectionStatus = 'disconnected';
	let moistureHistory: MoistureDataPoint[] = [];

	// Connect to backend and start polling for data
	async function connectToBackend(): Promise<void> {
		if (!browser) return;

		try {
			connecting = true;
			connectionError = null;

			// Check connection status using the provided endpoint
			const statusResponse = await fetch(`/api/iot/status`);
			if (!statusResponse.ok) {
				throw new Error('Failed to connect to backend server');
			}

			const statusData: StatusResponse = await statusResponse.json();
			connectionStatus = statusData.status;

			if (connectionStatus === 'connected') {
				console.log('Backend is connected to AWS IoT Core');

				// If we have a latest message from status, use it
				if (statusData.latestMessage && statusData.latestMessage.payload) {
					updateDeviceStatus(statusData.latestMessage.payload);
				}

				startPolling();
			} else {
				connectionError = 'Backend is not connected to AWS IoT Core';
				connecting = false;
			}
		} catch (error) {
			console.error('Failed to connect to backend:', error);
			connecting = false;
			connectionError = 'Failed to connect to backend: ' + (error as Error).message;
		}
	}

	// Poll for latest message
	async function startPolling(): Promise<void> {
		// Clear any existing interval
		if (pollingInterval) {
			clearInterval(pollingInterval);
		}

		// Immediately fetch data
		await fetchLatestStatus();

		// Set up polling interval (every 2 seconds)
		pollingInterval = setInterval(fetchLatestStatus, 1000);
	}

	// Fetch latest status from backend
	async function fetchLatestStatus(): Promise<void> {
		try {
			const response = await fetch(`/api/iot/status`);
			if (!response.ok) {
				throw new Error('Failed to fetch latest status');
			}

			const data: StatusResponse = await response.json();

			// Check if we have a valid message in the latestMessage field
			if (data.latestMessage && data.latestMessage.payload) {
				updateDeviceStatus(data.latestMessage.payload);
				connecting = false;
			} else if (!dataReceived) {
				// Still waiting for first message
				connecting = true;
			}
		} catch (error) {
			console.error('Error fetching latest status:', error);
			if (dataReceived) {
				// We've received data before, so this is just a temporary error
				connectionStatus = 'disconnected';
			} else {
				// We've never received data, so show an error
				connecting = false;
				connectionError = 'Error fetching data: ' + (error as Error).message;
			}
		}
	}

	// Update device status with received data
	function updateDeviceStatus(data: DeviceStatusData): void {
		deviceId = data.deviceId || deviceId;
		moisture = data.moisture !== undefined ? data.moisture : moisture;
		timestamp = data.timestamp || timestamp;
		threshold = data.threshold !== undefined ? data.threshold : threshold;
		pumpState = data.pumpState || pumpState;
		mode = data.mode || mode;
		lastUpdated = new Date();
		connectionStatus = 'connected';

		// Mark that we've received data
		if (!dataReceived) {
			dataReceived = true;
		}

		// Add to history for logs
		const newDataPoint = {
			timestamp: new Date(data.timestamp || new Date()),
			moisture: data.moisture !== undefined ? data.moisture : 0
		};

		moistureHistory = [...moistureHistory, newDataPoint].slice(-30); // Keep last 30 readings
		console.log('Added data point:', newDataPoint, 'Total points:', moistureHistory.length);
	}

	// Send command to device via backend API using the provided endpoint
	async function sendCommand(
		command: CommandType,
		state?: PumpState | DeviceMode | null,
		value?: number
	): Promise<void> {
		try {
			const commandData: CommandRequest = {
				command,
				state: state || null,
				value: value !== undefined ? value : null
			};

			const response = await fetch(`/api/iot/send-command`, {
				method: 'POST',
				headers: {
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(commandData)
			});

			if (!response.ok) {
				throw new Error('Failed to send command');
			}

			console.log(`Command sent: ${command}`);
		} catch (error) {
			console.error('Error sending command:', error);
		}
	}

	// Toggle pump state
	function togglePump(): void {
		const newState: PumpState = pumpState === 'on' ? 'off' : 'on';
		sendCommand('pump', newState);
	}

	// Toggle mode
	function toggleMode(): void {
		const newMode: DeviceMode = mode === 'auto' ? 'manual' : 'auto';
		sendCommand('mode', newMode);
	}

	// Update threshold
	function updateThreshold(event: Event): void {
		const target = event.target as HTMLInputElement;
		sendCommand('threshold', null, parseInt(target.value));
	}

	// Format time since last update
	function getTimeSinceUpdate(): string {
		if (!lastUpdated) return 'Never';

		const now = new Date();
		const diff = now.getTime() - lastUpdated.getTime();

		if (diff < 60000) {
			return 'Just now';
		} else if (diff < 3600000) {
			return `${Math.floor(diff / 60000)} min ago`;
		} else {
			return `${Math.floor(diff / 3600000)} hr ago`;
		}
	}

	// Format date time for logs
	function formatDateTime(date: Date): string {
		return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
	}

	// Get moisture status class
	function getMoistureStatusClass(value: number): string {
		if (value < threshold * 0.5) return 'text-red-400';
		if (value < threshold) return 'text-amber-400';
		if (value > 90) return 'text-blue-400';
		return 'text-emerald-400';
	}

	// Reconnect to backend
	function reconnect(): void {
		if (pollingInterval) {
			clearInterval(pollingInterval);
			pollingInterval = undefined;
		}

		connecting = true;
		connectionError = null;
		connectToBackend();
	}

	onMount(() => {
		console.log('Component mounted');
		connectToBackend();

		// Update "time since last update" every minute
		const updateInterval = setInterval(() => {
			if (lastUpdated) {
				lastUpdated = new Date(lastUpdated.getTime()); // Trigger reactivity with a new object
			}
		}, 60000);

		return () => {
			clearInterval(updateInterval);
			if (pollingInterval) {
				clearInterval(pollingInterval);
			}
		};
	});

	onDestroy(() => {
		if (pollingInterval) {
			clearInterval(pollingInterval);
		}
	});
</script>

<div class="min-h-screen bg-gray-900 p-4 text-white md:p-8">
	<div class="mx-auto max-w-7xl">
		<header class="mb-8 flex flex-col md:flex-row md:items-center md:justify-between">
			<div>
				<h1 class="text-3xl font-bold text-emerald-400 md:text-4xl">Plant Watering System</h1>
				<p class="text-gray-300">Real-time monitoring and control</p>
			</div>

			<div class="mt-4 flex items-center md:mt-0">
				<div class="mr-4 flex items-center">
					<div
						class={`mr-2 h-3 w-3 rounded-full ${connectionStatus === 'connected' ? 'bg-emerald-500' : 'bg-red-500'}`}
					></div>
					<span
						class={`text-sm font-medium ${connectionStatus === 'connected' ? 'text-emerald-400' : 'text-red-400'}`}
					>
						{connectionStatus === 'connected' ? 'Connected' : 'Disconnected'}
					</span>
				</div>

				{#if connectionStatus !== 'connected' && !connecting}
					<button
						on:click={reconnect}
						class="flex items-center rounded-md bg-emerald-600 px-3 py-1 text-sm text-white transition-colors hover:bg-emerald-700"
					>
						<RefreshCw class="mr-1 h-3 w-3" />
						Reconnect
					</button>
				{/if}
			</div>
		</header>

		{#if connecting && !dataReceived}
			<div class="flex h-64 flex-col items-center justify-center" in:fade>
				<Loader2 class="mb-4 h-12 w-12 animate-spin text-emerald-500" />
				<p class="text-lg text-gray-300">Connecting to server...</p>
			</div>
		{:else if connectionError && !dataReceived}
			<div class="mb-6 rounded-xl border border-red-800 bg-gray-800 p-6" in:fade>
				<div class="flex items-start">
					<AlertTriangle class="mr-3 mt-0.5 h-6 w-6 flex-shrink-0 text-red-400" />
					<div>
						<h3 class="text-lg font-medium text-red-400">Connection Error</h3>
						<p class="mt-1 text-red-300">{connectionError}</p>
						<button
							on:click={reconnect}
							class="mt-3 flex items-center rounded-md bg-red-600 px-4 py-2 text-white transition-colors hover:bg-red-700"
						>
							<RefreshCw class="mr-2 h-4 w-4" />
							Try Again
						</button>
					</div>
				</div>
			</div>
		{:else if !dataReceived}
			<div class="flex h-64 flex-col items-center justify-center" in:fade>
				<Loader2 class="mb-4 h-12 w-12 animate-spin text-emerald-500" />
				<p class="text-lg text-gray-300">Waiting for device data...</p>
			</div>
		{:else}
			<div class="grid grid-cols-1 gap-6 lg:grid-cols-3" in:fade={{ duration: 300 }}>
				<!-- Status Card -->
				<div
					class="col-span-1 rounded-xl bg-gray-800 p-6 shadow-lg"
					in:fly={{ y: 20, duration: 300, delay: 100 }}
				>
					<div class="mb-4 flex items-center justify-between">
						<h2 class="text-xl font-semibold text-white">System Status</h2>
						<span class="text-xs text-gray-400">Updated: {getTimeSinceUpdate()}</span>
					</div>

					<div class="space-y-5">
						<div class="flex items-center justify-between">
							<span class="text-gray-300">Device ID:</span>
							<span class="font-medium text-white">{deviceId}</span>
						</div>

						<div class="flex items-center justify-between">
							<span class="text-gray-300">Last Update:</span>
							<span class="font-medium text-white">
								{timestamp ? new Date(timestamp).toLocaleTimeString() : 'N/A'}
							</span>
						</div>

						<div class="flex items-center justify-between">
							<span class="text-gray-300">Mode:</span>
							<div class="flex items-center">
								<span class="mr-3 font-medium capitalize text-white">{mode}</span>
								<button
									on:click={toggleMode}
									class="relative inline-flex h-6 w-11 items-center rounded-full transition-colors focus:outline-none focus:ring-2 focus:ring-emerald-500 focus:ring-offset-2 focus:ring-offset-gray-800"
									class:bg-emerald-600={mode === 'auto'}
									class:bg-gray-600={mode === 'manual'}
								>
									<span
										class="inline-block h-4 w-4 transform rounded-full bg-white transition-transform"
										class:translate-x-6={mode === 'auto'}
										class:translate-x-1={mode === 'manual'}
									></span>
								</button>
							</div>
						</div>

						<div class="flex items-center justify-between">
							<span class="text-gray-300">Pump:</span>
							<div class="flex items-center">
								<span class="mr-3 font-medium capitalize text-white">{pumpState}</span>
								<button
									on:click={togglePump}
									class="relative inline-flex h-6 w-11 items-center rounded-full transition-colors focus:outline-none focus:ring-2 focus:ring-sky-500 focus:ring-offset-2 focus:ring-offset-gray-800"
									class:bg-sky-600={pumpState === 'on'}
									class:bg-gray-600={pumpState === 'off'}
									disabled={mode === 'auto'}
									title={mode === 'auto' ? 'Switch to manual mode to control pump' : ''}
								>
									<span
										class="inline-block h-4 w-4 transform rounded-full bg-white transition-transform"
										class:translate-x-6={pumpState === 'on'}
										class:translate-x-1={pumpState === 'off'}
									></span>
								</button>
							</div>
						</div>

						<div class="pt-2">
							<div class="mb-2 flex justify-between">
								<label for="threshold" class="block text-gray-300"> Moisture Threshold </label>
								<span class="font-medium text-white">{threshold}%</span>
							</div>
							<input
								id="threshold"
								type="range"
								min="0"
								max="100"
								bind:value={threshold}
								on:change={updateThreshold}
								class="h-2 w-full cursor-pointer appearance-none rounded-lg bg-gray-700 accent-emerald-500"
							/>
							<div class="mt-1 flex justify-between text-xs text-gray-400">
								<span>Dry (0%)</span>
								<span>Wet (100%)</span>
							</div>
						</div>
					</div>
				</div>

				<!-- Moisture Indicator Card -->
				<div
					class="col-span-1 rounded-xl bg-gray-800 p-6 shadow-lg"
					in:fly={{ y: 20, duration: 300, delay: 200 }}
				>
					<h2 class="mb-6 text-xl font-semibold text-white">Current Moisture</h2>

					<div class="flex h-64 flex-col items-center justify-center">
						<div class="relative h-48 w-48">
							<!-- Water drop background -->
							<div class="absolute inset-0 flex items-center justify-center">
								<Droplet class="h-48 w-48 text-sky-900" />
							</div>

							<!-- Circular background -->
							<div class="absolute inset-0 rounded-full border-8 border-gray-700"></div>

							<!-- Animated moisture level -->
							<div
								class="absolute bottom-0 left-0 right-0 overflow-hidden rounded-b-full bg-gradient-to-t from-sky-500 to-sky-400 transition-all duration-1000 ease-out"
								style="height: {moisture}%; opacity: 0.8;"
							>
								<div class="absolute inset-0 animate-pulse bg-white opacity-50"></div>
							</div>

							<!-- Threshold indicator -->
							<div
								class="absolute left-0 right-0 z-10 border-2 border-dashed border-red-500"
								style="bottom: {threshold}%;"
							></div>

							<!-- Value display -->
							<div class="absolute inset-0 flex items-center justify-center">
								<span class="text-4xl font-bold text-sky-300 drop-shadow-sm">{moisture}%</span>
							</div>
						</div>

						<div class="mt-4 text-center">
							<span
								class={moisture < threshold
									? 'font-medium text-red-400'
									: 'font-medium text-emerald-400'}
							>
								{moisture < threshold ? 'Below threshold' : 'Above threshold'}
							</span>
						</div>
					</div>
				</div>

				<!-- Logs Card (replacing Chart Card) -->
				<div
					class="col-span-1 rounded-xl bg-gray-800 p-6 shadow-lg lg:col-span-1"
					in:fly={{ y: 20, duration: 300, delay: 300 }}
				>
					<div class="mb-4 flex items-center justify-between">
						<h2 class="text-xl font-semibold text-white">Moisture History</h2>
						<span class="text-xs text-gray-400">Last {moistureHistory.length} readings</span>
					</div>

					{#if moistureHistory.length === 0}
						<div class="flex h-64 items-center justify-center">
							<p class="text-center text-gray-400">No historical data available yet</p>
						</div>
					{:else}
						<div
							class="scrollbar scrollbar-track-gray-700 scrollbar-thumb-gray-600 h-64 overflow-y-auto pr-2"
						>
							<table class="w-full">
								<thead class="sticky top-0 z-10 bg-gray-800">
									<tr
										class="border-b border-gray-700 text-left text-xs font-medium uppercase tracking-wider text-gray-400"
									>
										<th class="pb-2 pr-2">Time</th>
										<th class="pb-2 pr-2">Moisture</th>
										<th class="pb-2">Status</th>
									</tr>
								</thead>
								<tbody class="divide-y divide-gray-700">
									{#each [...moistureHistory].reverse() as record, i}
										<tr class="hover:bg-gray-750 text-sm">
											<td class="flex items-center py-2 pr-2">
												<Clock class="mr-1.5 h-3 w-3 text-gray-400" />
												<span class="text-gray-300">{formatDateTime(record.timestamp)}</span>
											</td>
											<td class="py-2 pr-2 font-medium text-white">{record.moisture}%</td>
											<td class="py-2">
												<span class={getMoistureStatusClass(record.moisture)}>
													{#if record.moisture < threshold * 0.5}
														Critically Low
													{:else if record.moisture < threshold}
														Below Threshold
													{:else if record.moisture > 90}
														Very Wet
													{:else}
														Good
													{/if}
												</span>
											</td>
										</tr>
									{/each}
								</tbody>
							</table>
						</div>
					{/if}
				</div>
			</div>

			<!-- Controls Card -->
			<div
				class="mt-6 rounded-xl bg-gray-800 p-6 shadow-lg"
				in:fly={{ y: 20, duration: 300, delay: 400 }}
			>
				<div class="mb-4 flex items-center">
					<Settings class="mr-2 h-5 w-5 text-gray-300" />
					<h2 class="text-xl font-semibold text-white">Control Panel</h2>
				</div>

				<div class="grid grid-cols-1 gap-6 md:grid-cols-2">
					<div class="rounded-lg bg-gray-700 p-4">
						<div class="mb-3 flex items-center">
							<Power class="mr-2 h-5 w-5 text-sky-400" />
							<h3 class="text-lg font-medium text-white">Pump Control</h3>
						</div>

						<button
							on:click={togglePump}
							class="flex w-full items-center justify-center rounded-lg px-6 py-3 font-medium transition-all focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-offset-gray-700"
							class:bg-sky-600={pumpState === 'on'}
							class:hover:bg-sky-700={pumpState === 'on'}
							class:text-white={pumpState === 'on'}
							class:bg-gray-600={pumpState === 'off'}
							class:hover:bg-gray-500={pumpState === 'off'}
							class:text-gray-200={pumpState === 'off'}
							disabled={mode === 'auto'}
							class:opacity-50={mode === 'auto'}
							class:cursor-not-allowed={mode === 'auto'}
						>
							<Power class="mr-2 h-5 w-5" />
							{pumpState === 'on' ? 'Turn Pump Off' : 'Turn Pump On'}
						</button>

						{#if mode === 'auto'}
							<p class="mt-2 flex items-center text-sm text-amber-400">
								<AlertTriangle class="mr-1 h-4 w-4" />
								Switch to manual mode to control the pump
							</p>
						{/if}
					</div>

					<div class="rounded-lg bg-gray-700 p-4">
						<div class="mb-3 flex items-center">
							<Settings class="mr-2 h-5 w-5 text-emerald-400" />
							<h3 class="text-lg font-medium text-white">Operation Mode</h3>
						</div>

						<button
							on:click={toggleMode}
							class="flex w-full items-center justify-center rounded-lg px-6 py-3 font-medium text-white transition-all focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-offset-gray-700"
							class:bg-emerald-600={mode === 'auto'}
							class:hover:bg-emerald-700={mode === 'auto'}
							class:bg-amber-500={mode === 'manual'}
							class:hover:bg-amber-600={mode === 'manual'}
						>
							<Settings class="mr-2 h-5 w-5" />
							{mode === 'auto' ? 'Switch to Manual Mode' : 'Switch to Auto Mode'}
						</button>

						<p class="mt-2 text-sm text-gray-300">
							{mode === 'auto'
								? 'System will automatically control the pump based on moisture threshold'
								: 'You have full control over the pump operation'}
						</p>
					</div>
				</div>
			</div>
		{/if}
	</div>
</div>

<style>
	@keyframes ripple {
		0% {
			transform: scale(1);
			opacity: 0.8;
		}
		50% {
			transform: scale(1.05);
			opacity: 0.5;
		}
		100% {
			transform: scale(1);
			opacity: 0.8;
		}
	}

	/* Custom scrollbar styles */
	.scrollbar {
		scrollbar-width: thin;
		scrollbar-color: #4b5563 #374151;
	}

	.scrollbar::-webkit-scrollbar {
		width: 6px;
	}

	.scrollbar::-webkit-scrollbar-track {
		background: #374151;
		border-radius: 3px;
	}

	.scrollbar::-webkit-scrollbar-thumb {
		background-color: #4b5563;
		border-radius: 3px;
	}

	.scrollbar::-webkit-scrollbar-thumb:hover {
		background-color: #6b7280;
	}

	/* Hover effect for table rows */
	tr.hover\:bg-gray-750:hover {
		background-color: #2d3748;
	}
</style>
