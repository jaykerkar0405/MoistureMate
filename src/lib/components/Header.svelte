<script lang="ts">
	import {  } from '@auth/sveltekit/client';

	let session = await getSession();
</script>

<header class="flex items-center justify-between bg-blue-600 p-4 text-white shadow-md">
	<div class="flex items-center gap-2">
		<img src="/logo.png" alt="Logo" class="h-10 w-10" />
		<span class="text-xl font-bold">MoistureMate</span>
	</div>

	{#if session?.user}
		<img
			alt="Profile"
			src={session.user.image}
			class="h-10 w-10 rounded-full border-2 border-white"
		/>
	{/if}
</header>
