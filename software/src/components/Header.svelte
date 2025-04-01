<script lang="ts">
	import { onMount } from 'svelte';

	let { session } = $props();
	let scrolled = $state(false);

	onMount(() => {
		const handleScroll = () => {
			scrolled = window.scrollY > 10;
		};

		window.addEventListener('scroll', handleScroll);
		return () => window.removeEventListener('scroll', handleScroll);
	});
</script>

<header
	class="fixed left-0 right-0 top-0 z-50 flex h-16 items-center justify-between border-b transition-all duration-300 {scrolled
		? 'border-gray-700 bg-gray-800/95 backdrop-blur-sm'
		: 'border-transparent bg-gray-800/80'} px-4 py-3 text-white"
>
	<div class="flex items-center gap-3">
		<img src="/pwa-512x512.png" alt="MoistureMate Logo" class="size-8" />

		<div class="flex flex-col">
			<span
				class="bg-gradient-to-r from-blue-400 to-cyan-300 bg-clip-text text-xl font-bold tracking-tight text-transparent"
			>
				MoistureMate
			</span>
			<span class="hidden text-xs text-gray-400 md:block">Smart Plant Hydration</span>
		</div>
	</div>

	{#if session?.user}
		<div class="flex items-center gap-4">
			<span class="hidden text-sm font-medium text-gray-300 sm:block">{session.user.name}</span>
			<div class="group relative">
				<div
					class="absolute inset-0 rounded-full bg-gradient-to-r from-blue-500 to-cyan-400 opacity-75 blur-sm transition-opacity group-hover:opacity-100"
				></div>
				<img
					alt="Profile"
					src={session.user.image}
					class="relative h-9 w-9 cursor-pointer rounded-full border-2 border-gray-800 shadow-md transition-all duration-300 group-hover:scale-105"
				/>
				<div
					class="absolute bottom-0 right-0 h-2.5 w-2.5 rounded-full border border-gray-800 bg-green-500 shadow-sm"
				></div>
			</div>
		</div>
	{:else}
		<a
			href="/auth"
			class="rounded-md bg-gradient-to-r from-blue-600 to-blue-500 px-6 py-2 text-base font-medium shadow-md transition-all duration-300 hover:from-blue-500 hover:to-cyan-500 hover:shadow-lg focus:outline-none focus:ring-2 focus:ring-blue-400 focus:ring-offset-2 focus:ring-offset-gray-800"
		>
			Sign In
		</a>
	{/if}
</header>

<style>
	:global(body) {
		overflow-x: hidden;
		margin: 0;
		padding: 0;
	}

	:global(#app) {
		position: relative;
		min-height: 100vh;
		padding-top: 4rem;
	}
</style>
