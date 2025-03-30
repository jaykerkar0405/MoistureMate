<script lang="ts">
	import { onMount } from 'svelte';

	interface Plant {
		id: string;
		name: string;
		publishTopic: string;
		subscribeTopic: string;
	}

	let plants: Plant[] = [];
	let isLoading: boolean = true;
	let isEditing: boolean = false;
	let error: string | null = null;
	let isModalOpen: boolean = false;
	let formElement: HTMLFormElement;
	let currentPlant: Plant = { id: '', name: '', subscribeTopic: '', publishTopic: '' };

	async function fetchPlants(): Promise<void> {
		try {
			isLoading = true;
			const response = await fetch('/api/plants');

			if (!response.ok) {
				throw new Error('Failed to fetch plants');
			}

			plants = await response.json();
		} catch (err) {
			error = err instanceof Error ? err.message : 'An unknown error occurred';
			console.error('Error fetching plants:', err);
		} finally {
			isLoading = false;
		}
	}

	async function createPlant(plantData: Omit<Plant, 'id'>): Promise<Plant> {
		try {
			const response = await fetch('/api/plants', {
				method: 'POST',
				headers: {
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(plantData)
			});

			if (!response.ok) {
				throw new Error('Failed to create plant');
			}

			const newPlant: Plant = await response.json();
			plants = [...plants, newPlant];
			return newPlant;
		} catch (err) {
			error = err instanceof Error ? err.message : 'An unknown error occurred';
			console.error('Error creating plant:', err);
			throw err;
		}
	}

	async function updatePlant(id: string, plantData: Omit<Plant, 'id'>): Promise<Plant> {
		try {
			const response = await fetch(`/api/plants/${id}`, {
				method: 'PUT',
				headers: {
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(plantData)
			});

			if (!response.ok) {
				throw new Error('Failed to update plant');
			}

			const updatedPlant: Plant = await response.json();
			plants = plants.map((p) => (p.id === id ? updatedPlant : p));
			return updatedPlant;
		} catch (err) {
			error = err instanceof Error ? err.message : 'An unknown error occurred';
			console.error('Error updating plant:', err);
			throw err;
		}
	}

	async function deletePlant(id: string): Promise<void> {
		try {
			const response = await fetch(`/api/plants/${id}`, {
				method: 'DELETE'
			});

			if (!response.ok) {
				throw new Error('Failed to delete plant');
			}

			plants = plants.filter((plant) => plant.id !== id);
		} catch (err) {
			error = err instanceof Error ? err.message : 'An unknown error occurred';
			console.error('Error deleting plant:', err);
		}
	}

	function openAddModal(): void {
		currentPlant = { id: '', name: '', subscribeTopic: '', publishTopic: '' };
		isEditing = false;
		isModalOpen = true;
	}

	function openEditModal(plant: Plant): void {
		currentPlant = { ...plant };
		isEditing = true;
		isModalOpen = true;
	}

	function closeModal(): void {
		isModalOpen = false;
		currentPlant = { id: '', name: '', subscribeTopic: '', publishTopic: '' };
	}

	async function handleSubmit(event: SubmitEvent): Promise<void> {
		event.preventDefault();
		if (!formElement) return;

		const formData = new FormData(formElement);

		const plantData = {
			name: formData.get('name') as string,
			publishTopic: formData.get('publishTopic') as string,
			subscribeTopic: formData.get('subscribeTopic') as string
		};

		try {
			if (isEditing) {
				await updatePlant(currentPlant.id, plantData);
			} else {
				await createPlant(plantData);
			}
			closeModal();
		} catch (err) {
			console.error('Error submitting form:', err);
		}
	}

	function handleDelete(id: string): void {
		if (confirm('Are you sure you want to delete this plant?')) {
			deletePlant(id);
		}
	}

	onMount(() => {
		fetchPlants();
	});
</script>

<div class="min-h-screen bg-gray-900 text-white">
	<div class="mx-auto max-w-7xl px-4 py-8 sm:px-6 lg:px-8">
		<header class="mb-8 flex items-center justify-between">
			<h1 class="text-3xl font-bold">Manage Plants</h1>
			<button
				class="rounded-md bg-blue-600 px-4 py-2 font-medium text-white transition-colors hover:bg-blue-700"
				on:click={openAddModal}
			>
				Add Plant
			</button>
		</header>

		<main>
			{#if isLoading}
				<div class="flex items-center justify-center py-12">
					<div
						class="h-12 w-12 animate-spin rounded-full border-t-2 border-b-2 border-blue-500"
					></div>
				</div>
			{:else if error}
				<div class="rounded-lg border border-red-700 bg-red-900/30 p-4 text-center">
					<p class="text-red-300">{error}</p>
					<button
						class="mt-2 rounded-md bg-red-700 px-3 py-1 text-sm text-white transition-colors hover:bg-red-800"
						on:click={fetchPlants}
					>
						Retry
					</button>
				</div>
			{:else if plants.length === 0}
				<div class="rounded-lg bg-gray-800 p-12 text-center">
					<p class="mb-4 text-gray-400">No plants added yet. Click "Add Plant" to get started.</p>
					<button
						class="rounded-md bg-blue-600 px-4 py-2 font-medium text-white transition-colors hover:bg-blue-700"
						on:click={openAddModal}
					>
						Add Plant
					</button>
				</div>
			{:else}
				<div class="overflow-hidden rounded-lg bg-gray-800 shadow-xl">
					<div class="overflow-x-auto">
						<table class="w-full">
							<thead class="bg-gray-700">
								<tr>
									<th
										class="px-6 py-3 text-left text-xs font-medium tracking-wider text-gray-300 uppercase"
										>Name</th
									>
									<th
										class="px-6 py-3 text-left text-xs font-medium tracking-wider text-gray-300 uppercase"
										>Subscribe Topic</th
									>
									<th
										class="px-6 py-3 text-left text-xs font-medium tracking-wider text-gray-300 uppercase"
										>Publish Topic</th
									>
									<th
										class="px-6 py-3 text-right text-xs font-medium tracking-wider text-gray-300 uppercase"
										>Actions</th
									>
								</tr>
							</thead>
							<tbody class="divide-y divide-gray-700">
								{#each plants as plant (plant.id)}
									<tr class="transition-colors hover:bg-gray-700/50">
										<td class="px-6 py-4 whitespace-nowrap">{plant.name}</td>
										<td class="px-6 py-4 font-mono text-sm whitespace-nowrap text-gray-300"
											>{plant.subscribeTopic}</td
										>
										<td class="px-6 py-4 font-mono text-sm whitespace-nowrap text-gray-300"
											>{plant.publishTopic}</td
										>
										<td class="px-6 py-4 text-right text-sm font-medium whitespace-nowrap">
											<div class="flex justify-end gap-2">
												<button
													class="rounded p-1 text-gray-400 transition-colors hover:bg-gray-700 hover:text-white"
													on:click={() => openEditModal(plant)}
													aria-label="Edit plant"
												>
													<svg
														xmlns="http://www.w3.org/2000/svg"
														class="h-5 w-5"
														viewBox="0 0 24 24"
														fill="none"
														stroke="currentColor"
														stroke-width="2"
														stroke-linecap="round"
														stroke-linejoin="round"
													>
														<path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"
														></path>
														<path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"
														></path>
													</svg>
												</button>
												<button
													class="rounded p-1 text-gray-400 transition-colors hover:bg-gray-700 hover:text-red-400"
													on:click={() => handleDelete(plant.id)}
													aria-label="Delete plant"
												>
													<svg
														xmlns="http://www.w3.org/2000/svg"
														class="h-5 w-5"
														viewBox="0 0 24 24"
														fill="none"
														stroke="currentColor"
														stroke-width="2"
														stroke-linecap="round"
														stroke-linejoin="round"
													>
														<path d="M3 6h18"></path>
														<path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6"></path>
														<path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
													</svg>
												</button>
											</div>
										</td>
									</tr>
								{/each}
							</tbody>
						</table>
					</div>
				</div>
			{/if}
		</main>
	</div>

	{#if isModalOpen}
		<div
			class="fixed inset-0 z-50 flex items-center justify-center bg-black/50"
			on:click={closeModal}
			role="dialog"
			aria-modal="true"
		>
			<div class="mx-4 w-full max-w-md rounded-lg bg-gray-800 shadow-xl" on:click|stopPropagation>
				<div class="flex items-center justify-between border-b border-gray-700 p-4">
					<h2 class="text-xl font-semibold">{isEditing ? 'Edit Plant' : 'Add New Plant'}</h2>
					<button
						class="rounded-full p-1 text-gray-400 transition-colors hover:bg-gray-700 hover:text-white"
						on:click={closeModal}
						aria-label="Close modal"
					>
						<svg
							xmlns="http://www.w3.org/2000/svg"
							class="h-6 w-6"
							viewBox="0 0 24 24"
							fill="none"
							stroke="currentColor"
							stroke-width="2"
							stroke-linecap="round"
							stroke-linejoin="round"
						>
							<line x1="18" y1="6" x2="6" y2="18"></line>
							<line x1="6" y1="6" x2="18" y2="18"></line>
						</svg>
					</button>
				</div>
				<div class="p-4">
					<form bind:this={formElement} on:submit={handleSubmit}>
						<div class="mb-4">
							<label for="name" class="mb-1 block text-sm font-medium">Plant Name</label>
							<input
								type="text"
								id="name"
								name="name"
								required
								value={currentPlant.name}
								class="w-full rounded-md border border-gray-600 bg-gray-700 px-3 py-2 text-white focus:border-blue-500 focus:ring-2 focus:ring-blue-500 focus:outline-none"
							/>
						</div>

						<div class="mb-4">
							<label for="subscribeTopic" class="mb-1 block text-sm font-medium"
								>Subscribe Topic</label
							>
							<input
								type="text"
								id="subscribeTopic"
								name="subscribeTopic"
								required
								value={currentPlant.subscribeTopic}
								placeholder="plants/[name]/status"
								class="w-full rounded-md border border-gray-600 bg-gray-700 px-3 py-2 text-white focus:border-blue-500 focus:ring-2 focus:ring-blue-500 focus:outline-none"
							/>
							<p class="mt-1 text-xs text-gray-400">Topic to receive plant status updates</p>
						</div>

						<div class="mb-4">
							<label for="publishTopic" class="mb-1 block text-sm font-medium">Publish Topic</label>
							<input
								type="text"
								id="publishTopic"
								name="publishTopic"
								required
								value={currentPlant.publishTopic}
								placeholder="plants/[name]/control"
								class="w-full rounded-md border border-gray-600 bg-gray-700 px-3 py-2 text-white focus:border-blue-500 focus:ring-2 focus:ring-blue-500 focus:outline-none"
							/>
							<p class="mt-1 text-xs text-gray-400">Topic to send control commands</p>
						</div>

						<div class="mt-6 flex justify-end gap-3">
							<button
								type="button"
								class="rounded-md border border-gray-600 px-4 py-2 text-gray-300 transition-colors hover:bg-gray-700"
								on:click={closeModal}
							>
								Cancel
							</button>
							<button
								type="submit"
								class="rounded-md bg-blue-600 px-4 py-2 font-medium text-white transition-colors hover:bg-blue-700"
							>
								{isEditing ? 'Update' : 'Add'} Plant
							</button>
						</div>
					</form>
				</div>
			</div>
		</div>
	{/if}
</div>
